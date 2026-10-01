#include "Combat/Ability/FEGrenadeAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

bool UFE_GrenadeAttackAbility::SupportsChargedAttackData(
	const UFE_ChargedProjectileAttackData* AttackData) const
{
	return AttackData && AttackData->IsA<UFE_GrenadeAttackData>();
}

void UFE_GrenadeAttackAbility::StartSpecializedChargePresentation()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_GrenadeAttackData* GrenadeData = Cast<UFE_GrenadeAttackData>(CachedChargedAttackData);
	if (!Character || !Character->IsLocallyControlled() || !GrenadeData || !Character->GetWorld())
	{
		return;
	}

	if (UNiagaraSystem* TrajectorySystem = GrenadeData->TrajectoryNiagaraSystem.Get())
	{
		TrajectoryComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrajectorySystem,
			Character->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			FVector::OneVector,
			EAttachLocation::SnapToTarget,
			false,
			ENCPoolMethod::ManualRelease,
			false,
			true);
		UpdateLocalTrajectory();
		if (TrajectoryComponent)
		{
			TrajectoryComponent->Activate(true);
			ScheduleTrajectoryUpdate();
		}
	}
}

void UFE_GrenadeAttackAbility::StopSpecializedChargePresentation()
{
	if (TrajectoryUpdateTask)
	{
		TrajectoryUpdateTask->EndTask();
		TrajectoryUpdateTask = nullptr;
	}
	if (TrajectoryComponent)
	{
		TrajectoryComponent->DeactivateImmediate();
		TrajectoryComponent->ReleaseToPool();
		TrajectoryComponent = nullptr;
	}
}

void UFE_GrenadeAttackAbility::HandleTrajectoryUpdate()
{
	TrajectoryUpdateTask = nullptr;
	if (!bReleaseRequested && IsActive())
	{
		UpdateLocalTrajectory();
		ScheduleTrajectoryUpdate();
	}
}

void UFE_GrenadeAttackAbility::UpdateLocalTrajectory()
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_GrenadeAttackData* GrenadeData = Cast<UFE_GrenadeAttackData>(CachedChargedAttackData);
	if (!Character || !TrajectoryComponent || !GrenadeData || !Character->GetWorld())
	{
		return;
	}

	FVector StartLocation;
	FVector LaunchDirection;
	if (!GetProjectileLaunchTransform(
		Character, GrenadeData, EFE_ProjectileLaunchContext::LocalPreview, StartLocation, LaunchDirection))
	{
		return;
	}

	const float LaunchSpeed = FMath::Lerp(
		FMath::Max(0.0f, GrenadeData->MinLaunchSpeed),
		FMath::Max(GrenadeData->MinLaunchSpeed, GrenadeData->MaxLaunchSpeed),
		CalculateChargeAlpha());

	FPredictProjectilePathParams Params;
	Params.StartLocation = StartLocation;
	Params.LaunchVelocity = LaunchDirection * LaunchSpeed;
	Params.ProjectileRadius = FMath::Max(0.0f, GrenadeData->TrajectoryCollisionRadius);
	Params.MaxSimTime = FMath::Max(0.1f, GrenadeData->TrajectorySimulationTime);
	Params.SimFrequency = FMath::Max(5.0f, GrenadeData->TrajectorySimulationFrequency);
	Params.OverrideGravityZ = Character->GetWorld()->GetGravityZ() * GrenadeData->GravityScale;
	Params.bTraceWithCollision = true;
	Params.bTraceComplex = false;
	Params.TraceChannel = GetAttackTraceChannel();
	Params.ActorsToIgnore.Add(const_cast<ACharacter*>(Character));

	FPredictProjectilePathResult Result;
	UGameplayStatics::PredictProjectilePath(Character, Params, Result);
	TArray<FVector> Points;
	Points.Reserve(FMath::Max(2, Result.PathData.Num()));
	const FTransform NiagaraTransform = TrajectoryComponent->GetComponentTransform();
	for (const FPredictProjectilePathPointData& Point : Result.PathData)
	{
		Points.Add(NiagaraTransform.InverseTransformPosition(Point.Location));
	}
	if (Points.IsEmpty())
	{
		Points.Add(NiagaraTransform.InverseTransformPosition(StartLocation));
	}
	if (Points.Num() == 1)
	{
		Points.Add(NiagaraTransform.InverseTransformPosition(StartLocation + LaunchDirection));
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		TrajectoryComponent, TEXT("User.TrajectoryPoints"), Points);
}

void UFE_GrenadeAttackAbility::ScheduleTrajectoryUpdate()
{
	const UFE_GrenadeAttackData* GrenadeData = Cast<UFE_GrenadeAttackData>(CachedChargedAttackData);
	if (!TrajectoryComponent || !GrenadeData || bReleaseRequested)
	{
		return;
	}
	TrajectoryUpdateTask = UAbilityTask_WaitDelay::WaitDelay(
		this, FMath::Max(0.016f, GrenadeData->TrajectoryUpdateInterval));
	TrajectoryUpdateTask->OnFinish.AddDynamic(this, &UFE_GrenadeAttackAbility::HandleTrajectoryUpdate);
	TrajectoryUpdateTask->ReadyForActivation();
}
