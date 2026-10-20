#include "Combat/Ability/FEMeleeAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEStaminaComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/Collision/FECameraMeleeTrace.h"
#include "Combat/Interface/FECombatPresentation.h"

ECollisionChannel UFE_MeleeAttackAbility::GetAttackTraceChannel() const
{
	return FECollisionChannels::PlayerMeleeTrace;
}

bool UFE_MeleeAttackAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) { return false; }
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UFE_StaminaComponent* Stamina = Avatar ? Avatar->FindComponentByClass<UFE_StaminaComponent>() : nullptr;
	const UFE_EquipmentComponent* Equipment = Avatar ? Avatar->FindComponentByClass<UFE_EquipmentComponent>() : nullptr;
	if (Stamina && Equipment && !Stamina->CanSpendStamina(Equipment->GetWeaponStaminaCost()))
	{
		// Report the reason to GAS; pure cost queries must not start UI animations themselves.
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(FallenEraCombatGameplayTags::Ability_ActivateFail_Stamina); }
		return false;
	}
	return !Stamina || Equipment != nullptr;
}

void UFE_MeleeAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UFallenEraGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bInputReleased = false;
	CurrentComboIndex = 0;
	AttackSelectionSeed = HashCombine(
		GetTypeHash(Handle),
		GetTypeHash(ActivationInfo.GetActivationPredictionKey()));
	AttackSelectionOrdinal = 0;
	bAttackAutomatic = false;
	bAttackInProgress = false;
	bAttackTraceConsumed = false;
	FirstHitsThisAttack.Reset();
	CachedComboAttacks.Reset();
	AreaTargetsHit = 0;

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? GetWeaponData(CombatCharacter) : nullptr;
	const UFE_MeleeAttackData* FirstAttack = Cast<UFE_MeleeAttackData>(ResolveAttackData(CombatCharacter));
	if (!CombatCharacter || !WeaponData || !FirstAttack || !CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	for (const UFE_WeaponAttackData* AttackData : WeaponData->Template.AttackActions)
	{
		UFE_MeleeAttackData* MeleeAttack = const_cast<UFE_MeleeAttackData*>(Cast<UFE_MeleeAttackData>(AttackData));
		if (MeleeAttack && MeleeAttack->InputTag == FirstAttack->InputTag &&
			MeleeAttack->AbilityClass.Get() == GetClass())
		{
			CachedComboAttacks.Add(MeleeAttack);
		}
	}
	if (CachedComboAttacks.IsEmpty())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (const UFE_MeleeWeaponItemData* MeleeWeaponData = Cast<UFE_MeleeWeaponItemData>(WeaponData);
		MeleeWeaponData && MeleeWeaponData->Template.AttackSequenceMode == EFE_MeleeAttackSequenceMode::RandomSingle)
	{
		CurrentComboIndex = SelectDeterministicRandomAttackIndex();
	}

	CacheAttackContext(WeaponData, CachedComboAttacks[CurrentComboIndex]);
	bAttackAutomatic = FirstAttack->bAutomatic;
	PerformCurrentAttack();

	if (bAttackAutomatic && IsActive())
	{
		InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
		InputReleaseTask->OnRelease.AddDynamic(this, &UFE_MeleeAttackAbility::OnInputReleased);
		InputReleaseTask->ReadyForActivation();
	}
}

void UFE_MeleeAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (AttackStartTask)
	{
		AttackStartTask->EndTask();
		AttackStartTask = nullptr;
	}
	if (AttackWindowTask)
	{
		AttackWindowTask->EndTask();
		AttackWindowTask = nullptr;
	}
	if (AttackResetTask)
	{
		AttackResetTask->EndTask();
		AttackResetTask = nullptr;
	}
	if (FallbackIntervalTask)
	{
		FallbackIntervalTask->EndTask();
		FallbackIntervalTask = nullptr;
	}
	bAttackTraceConsumed = true;
	FirstHitsThisAttack.Reset();
	WorldTargetThisAttack.Reset();
	bWorldTargetSelected = false;
	bAttackInProgress = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFE_MeleeAttackAbility::ExecuteAttack(
	const ACharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData) const
{
	const UFE_MeleeAttackData* MeleeAttackData = Cast<UFE_MeleeAttackData>(AttackData);
	if (!CombatCharacter || !WeaponData || !MeleeAttackData || !CombatCharacter->GetWorld())
	{
		return;
	}
	if (bWorldTargetSelected && MeleeAttackData->MaxHitCount > 0 &&
		AreaTargetsHit >= MeleeAttackData->MaxHitCount)
	{
		return;
	}
	const FFE_CombatAim Aim = IFE_CombatPresentation::ResolveCombatAim(
		CombatCharacter, MeleeAttackData->TraceRange, FECollisionChannels::PlayerHitscanTrace);
	const FVector ViewStart = Aim.Origin;
	const FRotator ViewRotation = Aim.Direction.Rotation();
	FFE_CameraMeleeTraceVolume Volume = FECameraMeleeTrace::MakeVolume(
		ViewStart, ViewRotation, MeleeAttackData->TraceRange, MeleeAttackData->TraceHalfExtent);
	TArray<FHitResult> Hits;
	FHitResult CenterHit;
	FECameraMeleeTrace::Sweep(*CombatCharacter->GetWorld(), CombatCharacter, Volume, GetAttackTraceChannel(), Hits, &CenterHit);
#if ENABLE_DRAW_DEBUG
	if (MeleeAttackData->bDrawDebugTrace)
	{
		const FColor Color = Hits.IsEmpty() ? FColor::Green : FColor::Red;
		const float Duration = FMath::Max(0.0f, MeleeAttackData->DebugTraceDuration);
		DrawDebugLine(CombatCharacter->GetWorld(), ViewStart,
			ViewStart + ViewRotation.Vector() * MeleeAttackData->TraceRange, Color, false, Duration);
		if (!Hits.IsEmpty() && Hits[0].bBlockingHit)
		{
			DrawDebugPoint(CombatCharacter->GetWorld(), Hits[0].ImpactPoint, 10.0f, FColor::Yellow, false, Duration);
		}
		if (Volume.bValid)
		{
			// Constant rotation and a straight sweep form one box spanning the full configured reach.
			FVector SweptHalfExtent = Volume.HalfExtent;
			SweptHalfExtent.X += FVector::Distance(Volume.Start, Volume.End) * 0.5f;
			DrawDebugBox(CombatCharacter->GetWorld(), (Volume.Start + Volume.End) * 0.5f,
				SweptHalfExtent, Volume.Rotation, Color, false, Duration);
		}
	}
#endif
	if (Hits.IsEmpty())
	{
		return;
	}

	for (const FHitResult& Hit : Hits)
	{
		AActor* TargetActor = Hit.GetActor();
		const bool bWorldTarget = !FECameraMeleeTrace::IsAreaTarget(Hit);
		if (bWorldTarget && bWorldTargetSelected &&
			(!TargetActor || WorldTargetThisAttack.Get() != TargetActor))
		{
			continue;
		}
		if (!TargetActor)
		{
			if (Hit.bBlockingHit)
			{
				bWorldTargetSelected = true;
				ApplyDamage(CombatCharacter, WeaponData, MeleeAttackData, nullptr, Hit);
			}
			continue;
		}
		const TWeakObjectPtr<AActor> TargetKey(TargetActor);
		const bool bAlreadyDamaged = FirstHitsThisAttack.Contains(TargetKey);
		if (bAlreadyDamaged)
		{
			continue;
		}
		if (!bWorldTarget && !bAlreadyDamaged && MeleeAttackData->MaxHitCount > 0 &&
			AreaTargetsHit >= MeleeAttackData->MaxHitCount)
		{
			continue;
		}
		FFE_MeleeHit* FirstHit = FirstHitsThisAttack.Find(TargetKey);
		if (!FirstHit)
		{
			const float QualityMultiplier = bWorldTarget || TargetActor == CenterHit.GetActor()
				? 1.0f : MeleeAttackData->IndirectDamageMultiplier;
			FirstHit = &FirstHitsThisAttack.Add(TargetKey,
				{ UFE_CombatComponent::RefineDirectDamageHit(Hit), QualityMultiplier });
			if (!bWorldTarget) { ++AreaTargetsHit; }
		}
		if (bWorldTarget)
		{
			bWorldTargetSelected = true;
			WorldTargetThisAttack = TargetActor;
		}
		ApplyDamage(CombatCharacter, WeaponData, MeleeAttackData, TargetActor, FirstHit->Hit, FirstHit->QualityMultiplier);
		if (bWorldTargetSelected && MeleeAttackData->MaxHitCount > 0 &&
			AreaTargetsHit >= MeleeAttackData->MaxHitCount)
		{
			break;
		}
	}
}

void UFE_MeleeAttackAbility::PerformCurrentAttack()
{
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = GetCachedWeaponData();
	const UFE_MeleeAttackData* AttackData = CachedComboAttacks.IsValidIndex(CurrentComboIndex)
		? CachedComboAttacks[CurrentComboIndex]
		: nullptr;
	if (!CombatCharacter || !WeaponData || !AttackData || bAttackInProgress)
	{
		return;
	}
	// Automatic/combo swings pay individually, not just once at ability activation.
	UFE_StaminaComponent* Stamina = CombatCharacter->FindComponentByClass<UFE_StaminaComponent>();
	const UFE_EquipmentComponent* Equipment = CombatCharacter->FindComponentByClass<UFE_EquipmentComponent>();
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo) ||
		(Stamina && (!Equipment || !Stamina->TrySpendStamina(Equipment->GetWeaponStaminaCost()))))
	{
		if (Stamina && Equipment && !Stamina->CanSpendStamina(Equipment->GetWeaponStaminaCost())) { Stamina->NotifyActionRejected(); }
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	bAttackInProgress = true;
	bAttackTraceConsumed = false;
	FirstHitsThisAttack.Reset();
	WorldTargetThisAttack.Reset();
	bWorldTargetSelected = false;
	CacheAttackContext(WeaponData, AttackData);
	AreaTargetsHit = 0;
	WaitForAttackEvents();
	PlayAttackPresentation(CombatCharacter, WeaponData, AttackData);
}

void UFE_MeleeAttackAbility::ExecuteCurrentTrace()
{
	if (bAttackTraceConsumed || !bAttackInProgress)
	{
		return;
	}
	// A miss also consumes the swing; later frames/notifies must not turn it into a hit.
	bAttackTraceConsumed = true;

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = GetCachedWeaponData();
	const UFE_MeleeAttackData* AttackData = Cast<UFE_MeleeAttackData>(GetCachedAttackData());
	if (CombatCharacter && CombatCharacter->HasAuthority() && WeaponData && AttackData)
	{
		ExecuteAttack(CombatCharacter, WeaponData, AttackData);
	}
}

void UFE_MeleeAttackAbility::WaitForAttackEvents()
{
	const UFE_MeleeAttackData* AttackData = CachedComboAttacks.IsValidIndex(CurrentComboIndex)
		? CachedComboAttacks[CurrentComboIndex]
		: nullptr;
	if (!AttackData)
	{
		return;
	}

	const FGameplayTag StartEventTag = AttackData->AttackStartEventTag;
	if (StartEventTag.IsValid())
	{
		AttackStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StartEventTag, nullptr, true, true);
		AttackStartTask->EventReceived.AddDynamic(this, &UFE_MeleeAttackAbility::OnAttackStartEvent);
		AttackStartTask->ReadyForActivation();
	}
	else
	{
		ExecuteCurrentTrace();
	}

	const FGameplayTag EndEventTag = AttackData->AttackEndEventTag;
	const FGameplayTag ResetEventTag = AttackData->AttackResetEventTag;
	if (EndEventTag.IsValid() && EndEventTag != StartEventTag && EndEventTag != ResetEventTag)
	{
		AttackWindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EndEventTag, nullptr, true, true);
		AttackWindowTask->EventReceived.AddDynamic(this, &UFE_MeleeAttackAbility::OnAttackEndEvent);
		AttackWindowTask->ReadyForActivation();
	}
	if (ResetEventTag.IsValid() && ResetEventTag != StartEventTag)
	{
		AttackResetTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ResetEventTag, nullptr, true, true);
		AttackResetTask->EventReceived.AddDynamic(this, &UFE_MeleeAttackAbility::OnAttackResetEvent);
		AttackResetTask->ReadyForActivation();
	}

	// Keep a bounded exit path even when a montage notify/tag is missing.
	FallbackIntervalTask = UAbilityTask_WaitDelay::WaitDelay(this, GetAttackResetDelay(AttackData));
	FallbackIntervalTask->OnFinish.AddDynamic(this, &UFE_MeleeAttackAbility::OnFallbackIntervalElapsed);
	FallbackIntervalTask->ReadyForActivation();
}

void UFE_MeleeAttackAbility::OnInputReleased(float TimeHeld)
{
	(void)TimeHeld;
	bInputReleased = true;
}

void UFE_MeleeAttackAbility::OnAttackStartEvent(FGameplayEventData Payload)
{
	(void)Payload;
	if (bAttackInProgress)
	{
		ExecuteCurrentTrace();
	}
}

void UFE_MeleeAttackAbility::OnAttackEndEvent(FGameplayEventData Payload)
{
	(void)Payload;
	if (!bAttackInProgress)
	{
		return;
	}

	if (AttackStartTask)
	{
		AttackStartTask->EndTask();
		AttackStartTask = nullptr;
	}
	if (AttackWindowTask)
	{
		AttackWindowTask->EndTask();
		AttackWindowTask = nullptr;
	}
	bAttackTraceConsumed = true;
	// Trace consumption and attack reset are separate. Keep the ability locked until the
	// reset event (or bounded fallback) allows the next attack.
}

void UFE_MeleeAttackAbility::OnAttackResetEvent(FGameplayEventData Payload)
{
	(void)Payload;
	CompleteCurrentAttack();
}

void UFE_MeleeAttackAbility::CompleteCurrentAttack()
{
	if (!bAttackInProgress)
	{
		return;
	}
	if (AttackStartTask)
	{
		AttackStartTask->EndTask();
		AttackStartTask = nullptr;
	}
	if (AttackWindowTask)
	{
		AttackWindowTask->EndTask();
		AttackWindowTask = nullptr;
	}
	if (AttackResetTask)
	{
		AttackResetTask->EndTask();
		AttackResetTask = nullptr;
	}
	if (FallbackIntervalTask)
	{
		FallbackIntervalTask->EndTask();
		FallbackIntervalTask = nullptr;
	}
	bAttackTraceConsumed = true;
	bAttackInProgress = false;
	if (bInputReleased || !bAttackAutomatic)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (const UFE_MeleeWeaponItemData* MeleeWeaponData = Cast<UFE_MeleeWeaponItemData>(GetCachedWeaponData());
		MeleeWeaponData && MeleeWeaponData->Template.AttackSequenceMode == EFE_MeleeAttackSequenceMode::RandomSingle)
	{
		CurrentComboIndex = SelectDeterministicRandomAttackIndex();
	}
	else
	{
		CurrentComboIndex = (CurrentComboIndex + 1) % CachedComboAttacks.Num();
	}
	PerformCurrentAttack();
}

int32 UFE_MeleeAttackAbility::SelectDeterministicRandomAttackIndex()
{
	if (CachedComboAttacks.IsEmpty())
	{
		return INDEX_NONE;
	}

	// Local-predicted client and authority receive the same spec handle and prediction
	// key. Deriving every selection from those values keeps montage choice identical
	// without adding an RPC or restarting the owner's predicted montage.
	const uint32 SelectionSeed = HashCombine(
		AttackSelectionSeed,
		GetTypeHash(AttackSelectionOrdinal++));
	FRandomStream RandomStream(static_cast<int32>(SelectionSeed));
	return RandomStream.RandRange(0, CachedComboAttacks.Num() - 1);
}

void UFE_MeleeAttackAbility::OnFallbackIntervalElapsed()
{
	CompleteCurrentAttack();
}

float UFE_MeleeAttackAbility::GetAttackResetDelay(const UFE_MeleeAttackData* AttackData) const
{
	if (!AttackData)
	{
		return 0.01f;
	}

	const float MontageLength = AttackData->AttackMontage
		? AttackData->AttackMontage->GetPlayLength()
		: 0.0f;
	return FMath::Max(0.01f, FMath::Max(AttackData->AttackInterval, MontageLength));
}
