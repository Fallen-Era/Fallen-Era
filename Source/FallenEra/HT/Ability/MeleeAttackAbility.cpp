#include "HT/Ability/MeleeAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "HT/Ability/Tasks/MeleeTraceTask.h"
#include "GameFramework/Character.h"
#include "HT/Weapon/WeaponItemData.h"

void UFE_MeleeAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UFallenEraGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bInputReleased = false;
	CurrentComboIndex = 0;
	bAttackAutomatic = false;
	bAttackInProgress = false;
	bMeleeTraceActive = false;
	DamagedActorsThisAttack.Reset();
	NextRepeatedHitTimes.Reset();
	CachedComboAttacks.Reset();

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? GetWeaponData(CombatCharacter) : nullptr;
	const UFE_MeleeAttackData* FirstAttack = Cast<UFE_MeleeAttackData>(ResolveAttackData(CombatCharacter));
	if (!CombatCharacter || !WeaponData || !FirstAttack || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	for (const UFE_WeaponAttackData* AttackData : WeaponData->AttackActions)
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
		MeleeWeaponData && MeleeWeaponData->AttackSequenceMode == EFE_MeleeAttackSequenceMode::RandomSingle)
	{
		CurrentComboIndex = FMath::RandRange(0, CachedComboAttacks.Num() - 1);
	}

	CacheAttackContext(WeaponData, CachedComboAttacks[CurrentComboIndex]);
	bAttackAutomatic = FirstAttack->bAutomatic;
	PerformCurrentAttack();

	if (bAttackAutomatic)
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
	StopMeleeTrace();
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
	const USkeletalMeshComponent* Mesh = CombatCharacter->GetMesh();
	const FVector Start = Mesh && Mesh->DoesSocketExist(MeleeAttackData->StartSocket)
		? Mesh->GetSocketLocation(MeleeAttackData->StartSocket)
		: CombatCharacter->GetActorLocation();
	const FVector End = Mesh && Mesh->DoesSocketExist(MeleeAttackData->EndSocket)
		? Mesh->GetSocketLocation(MeleeAttackData->EndSocket)
		: Start + CombatCharacter->GetActorForwardVector() * 100.0f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FE_PlayerMeleeAttack), false, CombatCharacter);
	QueryParams.bReturnPhysicalMaterial = true;
	TArray<FHitResult> Hits;
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(FMath::Max(0.0f, MeleeAttackData->TraceRadius));
	if (!CombatCharacter->GetWorld()->SweepMultiByChannel(
		Hits, Start, End, FQuat::Identity, TraceChannel, TraceShape, QueryParams))
	{
		return;
	}

	TSet<AActor*> HitActorsThisFrame;
	const float CurrentTime = CombatCharacter->GetWorld()->GetTimeSeconds();
	for (const FHitResult& Hit : Hits)
	{
		AActor* TargetActor = Hit.GetActor();
		if (!TargetActor)
		{
			if (Hit.bBlockingHit)
			{
				ApplyDamage(CombatCharacter, WeaponData, MeleeAttackData, nullptr, Hit);
				break;
			}
			continue;
		}
		if (HitActorsThisFrame.Contains(TargetActor))
		{
			continue;
		}
		HitActorsThisFrame.Add(TargetActor);

		const bool bAlreadyDamaged = DamagedActorsThisAttack.Contains(TargetActor);
		if (!MeleeAttackData->bAllowMultipleHits && bAlreadyDamaged)
		{
			continue;
		}
		if (!bAlreadyDamaged && MeleeAttackData->MaxHitCount > 0 &&
			DamagedActorsThisAttack.Num() >= MeleeAttackData->MaxHitCount)
		{
			continue;
		}
		if (MeleeAttackData->bAllowMultipleHits && bAlreadyDamaged)
		{
			const TWeakObjectPtr<AActor> TargetKey(TargetActor);
			const float* NextHitTime = NextRepeatedHitTimes.Find(TargetKey);
			if (NextHitTime && CurrentTime < *NextHitTime)
			{
				continue;
			}
		}

		ApplyDamage(CombatCharacter, WeaponData, MeleeAttackData, TargetActor, Hit);
		DamagedActorsThisAttack.Add(TargetActor);
		if (MeleeAttackData->bAllowMultipleHits)
		{
			NextRepeatedHitTimes.Add(
				TWeakObjectPtr<AActor>(TargetActor),
				CurrentTime + FMath::Max(0.01f, MeleeAttackData->RepeatedHitInterval));
		}
		else if (MeleeAttackData->MaxHitCount > 0 &&
			DamagedActorsThisAttack.Num() >= MeleeAttackData->MaxHitCount)
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

	bAttackInProgress = true;
	CacheAttackContext(WeaponData, AttackData);
	WaitForAttackEvents();
	PlayAttackMontage(CombatCharacter, AttackData);
}

void UFE_MeleeAttackAbility::StartMeleeTrace()
{
	if (bMeleeTraceActive)
	{
		return;
	}

	DamagedActorsThisAttack.Reset();
	NextRepeatedHitTimes.Reset();
	bMeleeTraceActive = true;
	MeleeTraceTask = UFE_MeleeTraceTask::StartMeleeTrace(this);
	MeleeTraceTask->OnTraceTick.AddDynamic(this, &UFE_MeleeAttackAbility::ExecuteCurrentTrace);
	MeleeTraceTask->ReadyForActivation();
}

void UFE_MeleeAttackAbility::StopMeleeTrace()
{
	if (MeleeTraceTask)
	{
		MeleeTraceTask->EndTask();
		MeleeTraceTask = nullptr;
	}

	bMeleeTraceActive = false;
	DamagedActorsThisAttack.Reset();
	NextRepeatedHitTimes.Reset();
}

void UFE_MeleeAttackAbility::ExecuteCurrentTrace()
{
	if (!bMeleeTraceActive || !bAttackInProgress)
	{
		return;
	}

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
		StartMeleeTrace();
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
		StartMeleeTrace();
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
	StopMeleeTrace();
	// Trace end and attack reset are separate. Keep the ability locked until the
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
	StopMeleeTrace();
	bAttackInProgress = false;
	if (bInputReleased || !bAttackAutomatic)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (const UFE_MeleeWeaponItemData* MeleeWeaponData = Cast<UFE_MeleeWeaponItemData>(GetCachedWeaponData());
		MeleeWeaponData && MeleeWeaponData->AttackSequenceMode == EFE_MeleeAttackSequenceMode::RandomSingle)
	{
		CurrentComboIndex = FMath::RandRange(0, CachedComboAttacks.Num() - 1);
	}
	else
	{
		CurrentComboIndex = (CurrentComboIndex + 1) % CachedComboAttacks.Num();
	}
	PerformCurrentAttack();
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
