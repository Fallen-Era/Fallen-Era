#include "Combat/Ability/FEHitscanAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/Weapon/FEWeaponItemData.h"

ECollisionChannel UFE_HitscanAttackAbility::GetAttackTraceChannel() const
{
	return FECollisionChannels::PlayerHitscanTrace;
}

void UFE_HitscanAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Skip UFE_PlayerAttackAbility::ActivateAbility because automatic fire must stay active until input release.
	UFallenEraGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bInputReleased = false;
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponAttackData* AttackData = ResolveAttackData(CombatCharacter);
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? GetWeaponData(CombatCharacter) : nullptr;
	const UFE_HitscanAttackData* HitscanAttackData = Cast<UFE_HitscanAttackData>(AttackData);
	if (!CombatCharacter || !WeaponData || !HitscanAttackData)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const float CurrentTime = CombatCharacter->GetWorld() ? CombatCharacter->GetWorld()->GetTimeSeconds() : 0.0f;
	if ((!HitscanAttackData->bAutomatic && CurrentTime < NextFireTime) ||
		!CheckCost(Handle, ActorInfo) ||
		!CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	CacheAttackContext(WeaponData, AttackData);

	if (!FireOnce())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
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

bool UFE_HitscanAttackAbility::FireOnce()
{
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = GetCachedWeaponData();
	const UFE_HitscanAttackData* AttackData = Cast<UFE_HitscanAttackData>(GetCachedAttackData());
	if (!CombatCharacter || !WeaponData || !AttackData || !CombatCharacter->GetWorld())
	{
		return false;
	}

	const float CurrentTime = CombatCharacter->GetWorld()->GetTimeSeconds();
	if (CurrentTime < NextFireTime)
	{
		// A quick re-press keeps the ability alive until the previous shot interval ends.
		return true;
	}

	// Cost belongs to a shot, not to the lifetime of an automatic-fire ability.
	// A future ammo Cost GameplayEffect will therefore consume one round here.
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		return false;
	}
	NextFireTime = CurrentTime + FMath::Max(0.01f, AttackData->FireInterval);

	PlayAttackPresentation(CombatCharacter, WeaponData, AttackData);
	if (!CombatCharacter->HasAuthority())
	{
		ApplyLocalRecoil(CombatCharacter, AttackData->RecoilSettings);
		return true;
	}

	FVector ViewStart = CombatCharacter->GetActorLocation();
	FRotator ViewRotation = CombatCharacter->GetActorRotation();
	if (const AController* Controller = CombatCharacter->GetController())
	{
		Controller->GetPlayerViewPoint(ViewStart, ViewRotation);
	}

	FVector TraceStart = ViewStart;
	bool bTraceFromMuzzle = false;
	if (const USkeletalMeshComponent* Mesh = CombatCharacter->GetMesh(); Mesh && Mesh->DoesSocketExist(AttackData->MuzzleSocketName))
	{
		TraceStart = Mesh->GetSocketLocation(AttackData->MuzzleSocketName);
		bTraceFromMuzzle = true;
	}

	const int32 PelletCount = FMath::Max(1, AttackData->PelletCount);
	const int32 MaxTargetsPerPellet = FMath::Max(1, AttackData->PenetrationCount + 1);
	const float TraceRange = FMath::Max(0.0f, AttackData->TraceRange);
	const float FinalSpreadAngle = CalculateSpreadAngle(
		CombatCharacter,
		AttackData,
		AttackData->SpreadAngle,
		AttackData->AccuracySettings);

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		// De-duplicate one actor within a pellet's penetration path, but allow
		// separate pellets to contribute damage to the same target.
		TSet<AActor*> DamagedActors;
		// Every pellet gets an independent, uniform cone sample. Intrinsic shotgun
		// spread stays active even when the Accuracy attribute is 1.
		const FVector AimDirection = ApplySpreadToDirection(ViewRotation.Vector(), FinalSpreadAngle);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FE_PlayerHitscanAttack), true, CombatCharacter);
		QueryParams.bReturnPhysicalMaterial = true;
		const ECollisionChannel HitscanTraceChannel = GetAttackTraceChannel();

		FVector AimPoint = ViewStart + AimDirection * TraceRange;
		if (bTraceFromMuzzle)
		{
			FHitResult CameraHit;
			if (CombatCharacter->GetWorld()->LineTraceSingleByChannel(
				CameraHit, ViewStart, AimPoint, HitscanTraceChannel, QueryParams))
			{
				AimPoint = CameraHit.ImpactPoint;
			}
		}

		const FVector ShotDirection = (AimPoint - TraceStart).GetSafeNormal(SMALL_NUMBER, AimDirection);
		const FVector End = TraceStart + ShotDirection * TraceRange;
		TArray<FHitResult> Hits;
		CombatCharacter->GetWorld()->LineTraceMultiByChannel(
			Hits, TraceStart, End, HitscanTraceChannel, QueryParams);

#if ENABLE_DRAW_DEBUG
		if (AttackData->bDrawDebugTrace)
		{
			const FHitResult* BlockingHit = Hits.FindByPredicate(
				[](const FHitResult& Hit) { return Hit.bBlockingHit; });
			const FVector DebugEnd = BlockingHit ? BlockingHit->ImpactPoint : End;
			const bool bHitAnything = !Hits.IsEmpty();
			DrawDebugLine(
				CombatCharacter->GetWorld(),
				TraceStart,
				DebugEnd,
				bHitAnything ? FColor::Red : FColor::Green,
				false,
				FMath::Max(0.0f, AttackData->DebugTraceDuration),
				0,
				FMath::Max(0.0f, AttackData->DebugTraceThickness));
			for (const FHitResult& DebugHit : Hits)
			{
				DrawDebugPoint(
					CombatCharacter->GetWorld(),
					DebugHit.ImpactPoint,
					8.0f,
					FColor::Yellow,
					false,
					FMath::Max(0.0f, AttackData->DebugTraceDuration));
			}
		}
#endif

		int32 AppliedTargetCount = 0;
		for (const FHitResult& Hit : Hits)
		{
			AActor* TargetActor = Hit.GetActor();
			if (!TargetActor)
			{
				if (Hit.bBlockingHit)
				{
					ApplyDamage(CombatCharacter, WeaponData, AttackData, nullptr, Hit);
					break;
				}
				continue;
			}
			if (DamagedActors.Contains(TargetActor))
			{
				continue;
			}

			ApplyDamage(CombatCharacter, WeaponData, AttackData, TargetActor, Hit);
			DamagedActors.Add(TargetActor);
			if (++AppliedTargetCount >= MaxTargetsPerPellet)
			{
				break;
			}
		}
	}
	CommitSpreadShot(CombatCharacter, AttackData, AttackData->AccuracySettings);
	ApplyLocalRecoil(CombatCharacter, AttackData->RecoilSettings);
	return true;
}

void UFE_HitscanAttackAbility::ScheduleNextShot()
{
	if (bInputReleased)
	{
		return;
	}

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_HitscanAttackData* AttackData = Cast<UFE_HitscanAttackData>(GetCachedAttackData());
	if (!AttackData || !CombatCharacter)
	{
		return;
	}

	const float CurrentTime = CombatCharacter->GetWorld() ? CombatCharacter->GetWorld()->GetTimeSeconds() : 0.0f;
	const float Delay = NextFireTime > CurrentTime
		? NextFireTime - CurrentTime
		: FMath::Max(0.01f, AttackData->FireInterval);
	FireDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.01f, Delay));
	FireDelayTask->OnFinish.AddDynamic(this, &UFE_HitscanAttackAbility::OnFireIntervalElapsed);
	FireDelayTask->ReadyForActivation();
}

void UFE_HitscanAttackAbility::OnFireIntervalElapsed()
{
	if (!bInputReleased && IsActive())
	{
		if (!FireOnce())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			return;
		}
		ScheduleNextShot();
	}
}

void UFE_HitscanAttackAbility::OnInputReleased(float TimeHeld)
{
	(void)TimeHeld;
	bInputReleased = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
