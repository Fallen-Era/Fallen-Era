#include "HT/Ability/ChargedProjectileAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Projectile/CombatProjectile.h"
#include "HT/Weapon/WeaponItemData.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

UFE_ChargedProjectileAttackAbility::UFE_ChargedProjectileAttackAbility()
{
	// A held re-press keeps retrying activation until FireInterval has elapsed.
	ActivationPolicy = EFallenEraAbilityActivationPolicy::WhileInputActive;
}

void UFE_ChargedProjectileAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UFallenEraGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bReleaseRequested = false;
	bProjectileFired = false;
	bChargePresentationActive = false;
	ReleasedChargeAlpha = 0.0f;
	CachedChargedAttackData = nullptr;

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? GetWeaponData(CombatCharacter) : nullptr;
	const UFE_ChargedProjectileAttackData* AttackData = Cast<UFE_ChargedProjectileAttackData>(ResolveAttackData(CombatCharacter));
	if (!CombatCharacter || !WeaponData || !AttackData ||
		!CanFireProjectile(CombatCharacter, AttackData) ||
		!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CacheAttackContext(WeaponData, AttackData);
	if (!CacheProjectileClass(AttackData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CachedChargedAttackData = const_cast<UFE_ChargedProjectileAttackData*>(AttackData);
	ChargeStartTime = CombatCharacter->GetWorld() ? CombatCharacter->GetWorld()->GetTimeSeconds() : 0.0f;

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	InputReleaseTask->OnRelease.AddDynamic(this, &UFE_ChargedProjectileAttackAbility::HandleInputReleased);
	InputReleaseTask->ReadyForActivation();

	PlayAttackMontage(CombatCharacter, AttackData->ChargeMontage);
	StartLocalChargePresentation();
}

void UFE_ChargedProjectileAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (InputReleaseTask)
	{
		InputReleaseTask->EndTask();
		InputReleaseTask = nullptr;
	}
	if (TrajectoryUpdateTask)
	{
		TrajectoryUpdateTask->EndTask();
		TrajectoryUpdateTask = nullptr;
	}

	StopLocalChargePresentation();
	CachedChargedAttackData = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFE_ChargedProjectileAttackAbility::HandleInputReleased(float TimeHeld)
{
	(void)TimeHeld;
	BeginRelease();
}

void UFE_ChargedProjectileAttackAbility::HandleTrajectoryUpdate()
{
	TrajectoryUpdateTask = nullptr;
	if (!bReleaseRequested && IsActive())
	{
		UpdateLocalTrajectory();
		ScheduleTrajectoryUpdate();
	}
}

void UFE_ChargedProjectileAttackAbility::BeginRelease()
{
	if (bReleaseRequested || !CachedChargedAttackData)
	{
		return;
	}

	bReleaseRequested = true;
	ReleasedChargeAlpha = CalculateChargeAlpha();
	StopLocalChargePresentation();

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	PlayAttackMontage(CombatCharacter, CachedChargedAttackData->ReleaseMontage);
	FireChargedProjectile();
}

void UFE_ChargedProjectileAttackAbility::FireChargedProjectile()
{
	if (bProjectileFired || !CachedChargedAttackData)
	{
		return;
	}

	bProjectileFired = true;
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = GetCachedWeaponData();
	if (!CombatCharacter || !WeaponData)
	{
		const bool bReplicateEndAbility = CombatCharacter && CombatCharacter->HasAuthority();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, true);
		return;
	}

	RecordProjectileFired(CombatCharacter, CachedChargedAttackData);

	// The predicting client only ends its local charge presentation. WaitInputRelease
	// forwards the release to the server, which owns the projectile spawn and then
	// replicates the final ability end back to this client.
	if (!CombatCharacter->HasAuthority())
	{
		return;
	}

	SpawnProjectile(
		CombatCharacter,
		WeaponData,
		CachedChargedAttackData,
		CalculateLaunchSpeed());
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

float UFE_ChargedProjectileAttackAbility::CalculateChargeAlpha() const
{
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UWorld* World = CombatCharacter ? CombatCharacter->GetWorld() : nullptr;
	if (!World || !CachedChargedAttackData)
	{
		return 0.0f;
	}

	const float HeldTime = FMath::Max(0.0f, World->GetTimeSeconds() - ChargeStartTime);
	return FMath::Clamp(HeldTime / FMath::Max(0.01f, CachedChargedAttackData->MaxChargeTime), 0.0f, 1.0f);
}

float UFE_ChargedProjectileAttackAbility::CalculateLaunchSpeed() const
{
	if (!CachedChargedAttackData)
	{
		return 0.0f;
	}

	return FMath::Lerp(
		FMath::Max(0.0f, CachedChargedAttackData->MinLaunchSpeed),
		FMath::Max(CachedChargedAttackData->MinLaunchSpeed, CachedChargedAttackData->MaxLaunchSpeed),
		ReleasedChargeAlpha);
}

void UFE_ChargedProjectileAttackAbility::StartLocalChargePresentation()
{
	ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!CombatCharacter || !CachedChargedAttackData || !CachedProjectileClass || !CombatCharacter->GetWorld())
	{
		return;
	}

	if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
	{
		Combat->StartChargeProjectilePresentation(
			CachedProjectileClass,
			CachedChargedAttackData->AttachmentTarget,
			CachedChargedAttackData->ChargeAttachSocketName,
			CachedChargedAttackData->ChargeAttachOffset,
			true);
		bChargePresentationActive = true;
	}

	if (!CombatCharacter->IsLocallyControlled())
	{
		return;
	}

	if (UNiagaraSystem* TrajectorySystem = CachedChargedAttackData->TrajectoryNiagaraSystem.Get())
	{
		TrajectoryComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			CombatCharacter,
			TrajectorySystem,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			FVector::OneVector,
			false,
			false,
			ENCPoolMethod::None,
			true);
		UpdateLocalTrajectory();
		if (TrajectoryComponent)
		{
			TrajectoryComponent->Activate(true);
		}
		ScheduleTrajectoryUpdate();
	}
}

void UFE_ChargedProjectileAttackAbility::StopLocalChargePresentation()
{
	if (bChargePresentationActive)
	{
		if (ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
			{
				Combat->StopChargeProjectilePresentation(true);
			}
		}
		bChargePresentationActive = false;
	}

	if (TrajectoryUpdateTask)
	{
		TrajectoryUpdateTask->EndTask();
		TrajectoryUpdateTask = nullptr;
	}
	if (TrajectoryComponent)
	{
		TrajectoryComponent->DeactivateImmediate();
		TrajectoryComponent->DestroyComponent();
		TrajectoryComponent = nullptr;
	}
}

void UFE_ChargedProjectileAttackAbility::UpdateLocalTrajectory()
{
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!CombatCharacter || !TrajectoryComponent || !CachedChargedAttackData || !CombatCharacter->GetWorld())
	{
		return;
	}

	FVector StartLocation;
	FVector LaunchDirection;
	if (!GetProjectileLaunchTransform(
		CombatCharacter,
		CachedChargedAttackData,
		EFE_ProjectileLaunchContext::LocalPreview,
		StartLocation,
		LaunchDirection))
	{
		return;
	}

	const float LaunchSpeed = FMath::Lerp(
		FMath::Max(0.0f, CachedChargedAttackData->MinLaunchSpeed),
		FMath::Max(CachedChargedAttackData->MinLaunchSpeed, CachedChargedAttackData->MaxLaunchSpeed),
		CalculateChargeAlpha());
	const FVector LaunchVelocity = LaunchDirection * LaunchSpeed;

	FPredictProjectilePathParams PredictionParams;
	PredictionParams.StartLocation = StartLocation;
	PredictionParams.LaunchVelocity = LaunchVelocity;
	PredictionParams.ProjectileRadius = FMath::Max(0.0f, CachedChargedAttackData->TrajectoryCollisionRadius);
	PredictionParams.MaxSimTime = FMath::Max(0.1f, CachedChargedAttackData->TrajectorySimulationTime);
	PredictionParams.SimFrequency = FMath::Max(5.0f, CachedChargedAttackData->TrajectorySimulationFrequency);
	PredictionParams.OverrideGravityZ =
		CombatCharacter->GetWorld()->GetGravityZ() * CachedChargedAttackData->GravityScale;
	PredictionParams.bTraceWithCollision = true;
	PredictionParams.bTraceComplex = false;
	PredictionParams.TraceChannel = TraceChannel;
	PredictionParams.ActorsToIgnore.Add(const_cast<ACharacter*>(CombatCharacter));

	FPredictProjectilePathResult PredictionResult;
	UGameplayStatics::PredictProjectilePath(CombatCharacter, PredictionParams, PredictionResult);

	TArray<FVector> TrajectoryPoints;
	TrajectoryPoints.Reserve(FMath::Max(2, PredictionResult.PathData.Num()));
	for (const FPredictProjectilePathPointData& PointData : PredictionResult.PathData)
	{
		TrajectoryPoints.Add(PointData.Location);
	}

	// Keep the ribbon valid even if prediction cannot produce a complete step.
	if (TrajectoryPoints.IsEmpty())
	{
		TrajectoryPoints.Add(StartLocation);
	}
	if (TrajectoryPoints.Num() == 1)
	{
		TrajectoryPoints.Add(StartLocation + LaunchDirection);
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		TrajectoryComponent,
		TEXT("User.TrajectoryPoints"),
		TrajectoryPoints);
}

void UFE_ChargedProjectileAttackAbility::ScheduleTrajectoryUpdate()
{
	if (!TrajectoryComponent || !CachedChargedAttackData || bReleaseRequested)
	{
		return;
	}

	TrajectoryUpdateTask = UAbilityTask_WaitDelay::WaitDelay(
		this, FMath::Max(0.016f, CachedChargedAttackData->TrajectoryUpdateInterval));
	TrajectoryUpdateTask->OnFinish.AddDynamic(this, &UFE_ChargedProjectileAttackAbility::HandleTrajectoryUpdate);
	TrajectoryUpdateTask->ReadyForActivation();
}
