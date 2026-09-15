#include "HT/Ability/MeleeAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "HT/Character/CombatCharacter.h"
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
	SequenceStep = 0;
	CachedComboAttacks.Reset();
	const AFE_CombatCharacter* CombatCharacter = Cast<AFE_CombatCharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? CombatCharacter->GetCurrentWeaponData() : nullptr;
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
			MeleeAttack->AbilityClass.LoadSynchronous() == GetClass())
		{
			CachedComboAttacks.Add(MeleeAttack);
		}
	}
	if (CachedComboAttacks.IsEmpty())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	SequenceSeed = GetTypeHash(FirstAttack->AttackTag);
	if (const UFE_MeleeWeaponItemData* MeleeWeaponData = Cast<UFE_MeleeWeaponItemData>(WeaponData);
		MeleeWeaponData && MeleeWeaponData->AttackSequenceMode == EFE_MeleeAttackSequenceMode::RandomSingle)
	{
		FRandomStream RandomStream(SequenceSeed);
		CurrentComboIndex = RandomStream.RandRange(0, CachedComboAttacks.Num() - 1);
	}

	CacheAttackContext(WeaponData, CachedComboAttacks[CurrentComboIndex]);
	PerformCurrentAttack();
	if (!FirstAttack->bAutomatic)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	InputReleaseTask->OnRelease.AddDynamic(this, &UFE_MeleeAttackAbility::OnInputReleased);
	InputReleaseTask->ReadyForActivation();
	WaitForNextAttack();
}

void UFE_MeleeAttackAbility::ExecuteAttack(
	const AFE_CombatCharacter* CombatCharacter,
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
	TArray<FHitResult> Hits;
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(FMath::Max(0.0f, MeleeAttackData->TraceRadius));
	if (!CombatCharacter->GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, TraceChannel, TraceShape, QueryParams))
	{
		return;
	}

	TSet<AActor*> DamagedActors;
	for (const FHitResult& Hit : Hits)
	{
		AActor* TargetActor = Hit.GetActor();
		if (!TargetActor || DamagedActors.Contains(TargetActor))
		{
			continue;
		}

		ApplyDamage(CombatCharacter, WeaponData, MeleeAttackData, TargetActor);
		DamagedActors.Add(TargetActor);
		if (!MeleeAttackData->bAllowMultipleHits ||
			(MeleeAttackData->MaxHitCount > 0 && DamagedActors.Num() >= MeleeAttackData->MaxHitCount))
		{
			break;
		}
	}
}

void UFE_MeleeAttackAbility::PerformCurrentAttack()
{
	const AFE_CombatCharacter* CombatCharacter = Cast<AFE_CombatCharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = GetCachedWeaponData();
	const UFE_MeleeAttackData* AttackData = CachedComboAttacks.IsValidIndex(CurrentComboIndex)
		? CachedComboAttacks[CurrentComboIndex]
		: nullptr;
	if (!CombatCharacter || !WeaponData || !AttackData)
	{
		return;
	}

	CacheAttackContext(WeaponData, AttackData);
	PlayAttackMontage(CombatCharacter, AttackData);
	if (CombatCharacter->HasAuthority())
	{
		ExecuteAttack(CombatCharacter, WeaponData, AttackData);
	}
}

void UFE_MeleeAttackAbility::WaitForNextAttack()
{
	if (bInputReleased || !CachedComboAttacks.IsValidIndex(CurrentComboIndex))
	{
		return;
	}

	const UFE_MeleeAttackData* AttackData = CachedComboAttacks[CurrentComboIndex];
	if (AttackData->ExecutionEventTag.IsValid())
	{
		AttackWindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, AttackData->ExecutionEventTag, nullptr, true, true);
		AttackWindowTask->EventReceived.AddDynamic(this, &UFE_MeleeAttackAbility::OnAttackWindowEvent);
		AttackWindowTask->ReadyForActivation();
	}
	else
	{
		FallbackIntervalTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.01f, AttackData->AttackInterval));
		FallbackIntervalTask->OnFinish.AddDynamic(this, &UFE_MeleeAttackAbility::OnFallbackIntervalElapsed);
		FallbackIntervalTask->ReadyForActivation();
	}
}

void UFE_MeleeAttackAbility::OnInputReleased(float TimeHeld)
{
	(void)TimeHeld;
	bInputReleased = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFE_MeleeAttackAbility::OnAttackWindowEvent(FGameplayEventData Payload)
{
	(void)Payload;
	if (bInputReleased || CachedComboAttacks.IsEmpty())
	{
		return;
	}

	if (const UFE_MeleeWeaponItemData* MeleeWeaponData = Cast<UFE_MeleeWeaponItemData>(GetCachedWeaponData());
		MeleeWeaponData && MeleeWeaponData->AttackSequenceMode == EFE_MeleeAttackSequenceMode::RandomSingle)
	{
		FRandomStream RandomStream(SequenceSeed + ++SequenceStep);
		CurrentComboIndex = RandomStream.RandRange(0, CachedComboAttacks.Num() - 1);
	}
	else
	{
		CurrentComboIndex = (CurrentComboIndex + 1) % CachedComboAttacks.Num();
	}
	PerformCurrentAttack();
	WaitForNextAttack();
}

void UFE_MeleeAttackAbility::OnFallbackIntervalElapsed()
{
	OnAttackWindowEvent(FGameplayEventData());
}
