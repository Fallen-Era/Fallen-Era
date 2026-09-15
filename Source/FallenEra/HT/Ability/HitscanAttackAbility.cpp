#include "HT/Ability/HitscanAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HT/Character/CombatCharacter.h"
#include "HT/Weapon/WeaponItemData.h"

void UFE_HitscanAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Skip UFE_PlayerAttackAbility::ActivateAbility because automatic fire must stay active until input release.
	UFallenEraGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bInputReleased = false;
	const AFE_CombatCharacter* CombatCharacter = Cast<AFE_CombatCharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponAttackData* AttackData = ResolveAttackData(CombatCharacter);
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? CombatCharacter->GetCurrentWeaponData() : nullptr;
	const UFE_HitscanAttackData* HitscanAttackData = Cast<UFE_HitscanAttackData>(AttackData);
	if (!CombatCharacter || !WeaponData || !HitscanAttackData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	CacheAttackContext(WeaponData, AttackData);

	FireOnce();
	if (!HitscanAttackData->bAutomatic)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	InputReleaseTask->OnRelease.AddDynamic(this, &UFE_HitscanAttackAbility::OnInputReleased);
	InputReleaseTask->ReadyForActivation();
	ScheduleNextShot();
}

void UFE_HitscanAttackAbility::FireOnce()
{
	const AFE_CombatCharacter* CombatCharacter = Cast<AFE_CombatCharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = GetCachedWeaponData();
	const UFE_HitscanAttackData* AttackData = Cast<UFE_HitscanAttackData>(GetCachedAttackData());
	if (!CombatCharacter || !WeaponData || !AttackData || !CombatCharacter->GetWorld())
	{
		return;
	}
	PlayAttackMontage(CombatCharacter, AttackData);
	if (!CombatCharacter->HasAuthority())
	{
		return;
	}

	FVector Start = CombatCharacter->GetActorLocation();
	FRotator ViewRotation = CombatCharacter->GetActorRotation();
	if (const AController* Controller = CombatCharacter->GetController())
	{
		Controller->GetPlayerViewPoint(Start, ViewRotation);
	}

	if (const USkeletalMeshComponent* Mesh = CombatCharacter->GetMesh(); Mesh && Mesh->DoesSocketExist(AttackData->MuzzleSocketName))
	{
		Start = Mesh->GetSocketLocation(AttackData->MuzzleSocketName);
	}

	TSet<AActor*> DamagedActors;
	const int32 PelletCount = FMath::Max(1, AttackData->PelletCount);
	const int32 MaxTargetsPerPellet = FMath::Max(1, AttackData->PenetrationCount + 1);
	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		FVector Direction = ViewRotation.Vector();
		if (AttackData->SpreadAngle > 0.0f)
		{
			// Use a fresh random sample for every pellet. The previous fixed-seed stream
			// repeated the exact same spread pattern on every shot.
			Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(AttackData->SpreadAngle));
		}

		const FVector End = Start + Direction * FMath::Max(0.0f, AttackData->TraceRange);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FE_PlayerHitscanAttack), true, CombatCharacter);
		TArray<FHitResult> Hits;
		CombatCharacter->GetWorld()->LineTraceMultiByChannel(Hits, Start, End, TraceChannel, QueryParams);

		int32 AppliedTargetCount = 0;
		for (const FHitResult& Hit : Hits)
		{
			AActor* TargetActor = Hit.GetActor();
			if (!TargetActor || DamagedActors.Contains(TargetActor))
			{
				continue;
			}

			ApplyDamage(CombatCharacter, WeaponData, AttackData, TargetActor);
			DamagedActors.Add(TargetActor);
			if (++AppliedTargetCount >= MaxTargetsPerPellet)
			{
				break;
			}
		}
	}
}

void UFE_HitscanAttackAbility::ScheduleNextShot()
{
	if (bInputReleased)
	{
		return;
	}

	const UFE_HitscanAttackData* AttackData = Cast<UFE_HitscanAttackData>(GetCachedAttackData());
	if (!AttackData)
	{
		return;
	}

	FireDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.01f, AttackData->FireInterval));
	FireDelayTask->OnFinish.AddDynamic(this, &UFE_HitscanAttackAbility::OnFireIntervalElapsed);
	FireDelayTask->ReadyForActivation();
}

void UFE_HitscanAttackAbility::OnFireIntervalElapsed()
{
	if (!bInputReleased && IsActive())
	{
		FireOnce();
		ScheduleNextShot();
	}
}

void UFE_HitscanAttackAbility::OnInputReleased(float TimeHeld)
{
	(void)TimeHeld;
	bInputReleased = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
