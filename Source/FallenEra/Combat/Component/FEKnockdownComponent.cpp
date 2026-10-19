#include "Combat/Component/FEKnockdownComponent.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/Animation/FEEnemyAnimInstance.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEProceduralHitReactionComponent.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/Damage/FEHitZoneMappingData.h"
#include "Combat/FECombatGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "TimerManager.h"

UFE_KnockdownComponent::UFE_KnockdownComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UFE_KnockdownComponent::Configure(const FFE_KnockdownSettings& NewSettings)
{
	Settings = NewSettings;
	Character = Cast<ACharacter>(GetOwner());
	bConfigured = Character.IsValid();
	// Replication can arrive before the selected AI preset has finished loading.
	if (bConfigured)
	{
		// Local and replicated transitions commit before the mesh's normal animation evaluation.
		Character->GetMesh()->AddTickPrerequisiteComponent(this);
		if (State.Phase != EFE_KnockdownPhase::None || AppliedPhase != EFE_KnockdownPhase::None) { QueueStateApplication(); }
	}
}

void UFE_KnockdownComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFE_KnockdownComponent, State);
}

bool UFE_KnockdownComponent::TryStartFromHit(FName HitBone, const FVector& Direction)
{
	ACharacter* OwnerCharacter = Character.Get();
	if (!bConfigured || bDeathStopped || !OwnerCharacter || !OwnerCharacter->HasAuthority() ||
		!Settings.bEnabled || !UFE_CombatComponent::IsActorAlive(OwnerCharacter)) { return false; }
	// Consume repeats without restarting the timer or injecting another impulse.
	if (IsKnockedDown()) { return true; }
	const UFE_CombatComponent* Combat = OwnerCharacter->FindComponentByClass<UFE_CombatComponent>();
	const UFE_HitZoneMappingData* Mapping = Combat ? Combat->GetLoadedHitZoneMappingData() : nullptr;
	USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
	// ALS requires a non-simulating root body so physics never rotates the mesh's coordinate frame.
	const UPhysicsAsset* PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	const int32 RootBodyIndex = PhysicsAsset ? PhysicsAsset->FindBodyIndex(Mesh->GetBoneName(0)) : INDEX_NONE;
	const EFE_HitZone* Zone = Mesh && Mapping ? Mapping->FindHitZone(HitBone, Mesh->GetSkeletalMeshAsset()) : nullptr;
	const UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(OwnerCharacter);
	if (!Zone || *Zone != EFE_HitZone::Legs || !Mesh->GetBodyInstance(Settings.PelvisBone) ||
		Settings.PelvisBone == Mesh->GetBoneName(0) ||
		RootBodyIndex == INDEX_NONE || PhysicsAsset->SkeletalBodySetups[RootBodyIndex]->PhysicsType != PhysType_Kinematic ||
		(ASC && ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Immune_PhysicalReaction))) { return false; }

	State = {};
	State.Phase = EFE_KnockdownPhase::Ragdoll;
	State.HitBone = HitBone;
	State.Direction = Direction.GetSafeNormal();
	SetKnockdownTag(true);
	if (UAbilitySystemComponent* MutableASC = UFE_CombatComponent::FindAbilitySystemComponent(OwnerCharacter))
	{
		FGameplayTagContainer ActionTags(FallenEraCombatGameplayTags::Ability_Combat_Attack);
		ActionTags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Aim);
		MutableASC->CancelAbilities(&ActionTags);
	}
	if (UFE_CombatComponent* MutableCombat = OwnerCharacter->FindComponentByClass<UFE_CombatComponent>())
	{
		MutableCombat->StopWeaponActionPresentation(false);
	}
	if (AController* Controller = OwnerCharacter->GetController()) { Controller->StopMovement(); }
	ApplyState();
	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::RequestGetUp,
		FMath::Max(0.1f, Settings.DownDuration), false);
	OwnerCharacter->ForceNetUpdate();
	return true;
}

void UFE_KnockdownComponent::OnRep_State()
{
	QueueStateApplication();
}

void UFE_KnockdownComponent::QueueStateApplication()
{
	bPendingStateApplication = true;
	RefreshTransitionTick();
}

void UFE_KnockdownComponent::RefreshTransitionTick()
{
	const bool bPending = bPendingStateApplication || bGetUpRequested || bFinishGetUpRequested;
	const bool bFollowRagdoll = GetOwner()->HasAuthority() && bPresentationActive;
	// A transition must not wait for the old 0.1-second ragdoll-follow cooldown.
	if (bPending) { SetComponentTickIntervalAndCooldown(0.0f); }
	else { SetComponentTickInterval(0.1f); }
	SetComponentTickEnabled(!bDeathStopped && bConfigured && (bPending || bFollowRagdoll));
}

void UFE_KnockdownComponent::RequestGetUp()
{
	bGetUpRequested = true;
	RefreshTransitionTick();
}

void UFE_KnockdownComponent::RequestFinishGetUp()
{
	bFinishGetUpRequested = true;
	RefreshTransitionTick();
}

void UFE_KnockdownComponent::ApplyState()
{
	ACharacter* OwnerCharacter = Character.Get();
	if (!bConfigured || !OwnerCharacter || bDeathStopped || !UFE_CombatComponent::IsActorAlive(OwnerCharacter)) { return; }
	if (State.Phase == EFE_KnockdownPhase::Ragdoll)
	{
		if (!bPresentationActive) { EnterRagdoll(); }
		AppliedPhase = State.Phase;
		return;
	}
	// A newly relevant, already recovered actor must keep its current replicated position.
	if (State.Phase == EFE_KnockdownPhase::None && AppliedPhase == EFE_KnockdownPhase::None && !bPresentationActive) { return; }
	if (State.Phase == EFE_KnockdownPhase::GettingUp && AppliedPhase == State.Phase) { return; }
	// A client missing GettingUp still needs recovery alignment; normal completion must not teleport again.
	USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
	UFE_EnemyAnimInstance* EnemyAnim = Cast<UFE_EnemyAnimInstance>(Mesh->GetAnimInstance());
	if (State.Phase == EFE_KnockdownPhase::GettingUp && !bPresentationActive && !bGetUpPresentationActive)
	{
		// A newly relevant client may first see this actor halfway through GettingUp.
		bOriginalURO = Mesh->bEnableUpdateRateOptimizations;
		OriginalAnimTickOption = Mesh->VisibilityBasedAnimTickOption;
	}
	const float MontageElapsedTime = FMath::Max(0.0f, GetServerTime() - State.GetUpStartTime);
	const bool bBlendSnapshot = State.Phase == EFE_KnockdownPhase::GettingUp && bPresentationActive &&
		State.GetUpMontage.IsValid() && MontageElapsedTime < Settings.GetUpPoseBlendDuration &&
		EnemyAnim && EnemyAnim->CaptureRagdollPose();
	// As in ALS, prepare the get-up montage while the last physical pose still exists.
	if (State.Phase == EFE_KnockdownPhase::GettingUp)
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (UAnimMontage* Montage = State.GetUpMontage.Get())
			{
				const float Position = MontageElapsedTime * Montage->RateScale;
				if (Position < Montage->GetPlayLength()) { Anim->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, Position); }
			}
		}
	}
	const bool bNeedsRecoveryAlignment = State.Phase == EFE_KnockdownPhase::GettingUp || bPresentationActive;
	RestoreMesh();
	if (State.bHasRecoveryTransform && bNeedsRecoveryAlignment)
	{
		OwnerCharacter->SetActorLocationAndRotation(State.RecoveryLocation, State.RecoveryRotation,
			false, nullptr, ETeleportType::TeleportPhysics);
	}
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (State.Phase == EFE_KnockdownPhase::GettingUp)
	{
		Movement->DisableMovement();
		// Do not let visibility/URO skip the first recovery frames or leave the montage unevaluated.
		bGetUpPresentationActive = true;
		Mesh->bEnableUpdateRateOptimizations = false;
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		if (bBlendSnapshot) { EnemyAnim->BeginGetUpPoseBlend(Settings.GetUpPoseBlendDuration, MontageElapsedTime); }
		else if (EnemyAnim) { EnemyAnim->ClearGetUpPoseBlend(); }
	}
	else if (State.bHasRecoveryTransform)
	{
		ClearGetUpPresentation();
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(OriginalMovementMode == MOVE_None ? MOVE_Walking : OriginalMovementMode,
			OriginalCustomMovementMode);
	}
	AppliedPhase = State.Phase;
}

void UFE_KnockdownComponent::EnterRagdoll()
{
	ACharacter* OwnerCharacter = Character.Get();
	USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
	if (!Mesh->GetBodyInstance(Settings.PelvisBone)) { return; }
	ClearGetUpPresentation();
	if (UFE_EnemyAnimInstance* Anim = Cast<UFE_EnemyAnimInstance>(Mesh->GetAnimInstance())) { Anim->BeginRagdollPresentation(); }
	if (UFE_ProceduralHitReactionComponent* Physical = OwnerCharacter->FindComponentByClass<UFE_ProceduralHitReactionComponent>())
	{
		Physical->SetKnockdownSuppressed(true);
	}
	OriginalMeshTransform = Mesh->GetRelativeTransform();
	OriginalMeshCollision = Mesh->GetCollisionEnabled();
	OriginalMeshResponses = Mesh->GetCollisionResponseToChannels();
	OriginalCapsuleCollision = OwnerCharacter->GetCapsuleComponent()->GetCollisionEnabled();
	OriginalAnimTickOption = Mesh->VisibilityBasedAnimTickOption;
	bOriginalURO = Mesh->bEnableUpdateRateOptimizations;
	bOriginalBlendPhysics = Mesh->bBlendPhysics;
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	OriginalMovementMode = Movement->MovementMode;
	OriginalCustomMovementMode = Movement->CustomMovementMode;
	Movement->StopMovementImmediately();
	Movement->DisableMovement();
	OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UAnimInstance* Anim = Mesh->GetAnimInstance()) { Anim->Montage_Stop(0.0f); }
	Mesh->bEnableUpdateRateOptimizations = false;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// Preserve all weapon trace responses while adding support from floors and movable geometry.
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Mesh->SetCollisionResponseToChannel(FECollisionChannels::BuildPiece, ECR_Block);
	// Keep the mesh attached and its root kinematic; only the pelvis and descendants fall.
	Mesh->SetAllBodiesBelowSimulatePhysics(Settings.PelvisBone, true, true);
	Mesh->SetAllBodiesPhysicsBlendWeight(1.0f);
	Mesh->WakeAllRigidBodies();
	FName ImpulseBone = Mesh->GetBodyInstance(State.HitBone) ? State.HitBone : Settings.PelvisBone;
	if (FBodyInstance* Body = Mesh->GetBodyInstance(ImpulseBone); Body && Body->IsInstanceSimulatingPhysics())
	{
		Mesh->AddImpulse(FVector(State.Direction) * FMath::Max(0.0f, Settings.ImpulseStrength), ImpulseBone, true);
	}
	bPresentationActive = true;
	RefreshTransitionTick();
}

void UFE_KnockdownComponent::RestoreMesh()
{
	ACharacter* OwnerCharacter = Character.Get();
	if (!bPresentationActive || !OwnerCharacter) { return; }
	USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
	Mesh->SetAllBodiesSimulatePhysics(false);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetAllBodiesPhysicsBlendWeight(0.0f);
	Mesh->bBlendPhysics = bOriginalBlendPhysics;
	Mesh->SetRelativeTransform(OriginalMeshTransform);
	Mesh->SetCollisionResponseToChannels(OriginalMeshResponses);
	Mesh->SetCollisionEnabled(OriginalMeshCollision);
	Mesh->bEnableUpdateRateOptimizations = bOriginalURO;
	Mesh->VisibilityBasedAnimTickOption = OriginalAnimTickOption;
	OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(OriginalCapsuleCollision);
	if (UFE_ProceduralHitReactionComponent* Physical = OwnerCharacter->FindComponentByClass<UFE_ProceduralHitReactionComponent>())
	{
		Physical->SetKnockdownSuppressed(false);
	}
	bPresentationActive = false;
	SetComponentTickEnabled(false);
}

void UFE_KnockdownComponent::ClearGetUpPresentation()
{
	if (ACharacter* OwnerCharacter = Character.Get())
	{
		USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
		if (UFE_EnemyAnimInstance* Anim = Cast<UFE_EnemyAnimInstance>(Mesh->GetAnimInstance())) { Anim->ClearGetUpPoseBlend(); }
		if (bGetUpPresentationActive)
		{
			Mesh->bEnableUpdateRateOptimizations = bOriginalURO;
			Mesh->VisibilityBasedAnimTickOption = OriginalAnimTickOption;
		}
	}
	bGetUpPresentationActive = false;
}

bool UFE_KnockdownComponent::FindRecoveryLocation(FVector& OutLocation) const
{
	const ACharacter* OwnerCharacter = Character.Get();
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Pelvis = OwnerCharacter->GetMesh()->GetSocketLocation(Settings.PelvisBone);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FEKnockdownRecovery), false, OwnerCharacter);
	FCollisionObjectQueryParams FloorObjects;
	FloorObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	FloorObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FloorObjects.AddObjectTypesToQuery(FECollisionChannels::BuildPiece);
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const float Angle = (Index - 1) * PI / 4.0f;
		const FVector Offset = Index == 0 ? FVector::ZeroVector : FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius * 2.0f;
		const FVector Probe = Pelvis + Offset;
		FHitResult Floor;
		if (!GetWorld()->LineTraceSingleByObjectType(Floor, Probe + FVector(0.0f, 0.0f, HalfHeight),
			Probe - FVector(0.0f, 0.0f, HalfHeight * 2.0f + 200.0f), FloorObjects, Params) ||
			!OwnerCharacter->GetCharacterMovement()->IsWalkable(Floor)) { continue; }
		FVector FloorLocation = Floor.ImpactPoint;
		if (Navigation && Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
		{
			FNavLocation NavLocation;
			if (!Navigation->ProjectPointToNavigation(FloorLocation, NavLocation, FVector(Radius, Radius, HalfHeight))) { continue; }
			FloorLocation = NavLocation.Location;
		}
		const FVector Candidate = FloorLocation + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
		if (!GetWorld()->OverlapBlockingTestByProfile(Candidate, FQuat::Identity, Capsule->GetCollisionProfileName(),
			FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
		{
			OutLocation = Candidate;
			return true;
		}
	}
	return false;
}

void UFE_KnockdownComponent::BeginGetUp()
{
	ACharacter* OwnerCharacter = Character.Get();
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority() || State.Phase != EFE_KnockdownPhase::Ragdoll) { return; }
	if (bDeathStopped || !UFE_CombatComponent::IsActorAlive(OwnerCharacter)) { StopForDeath(); return; }
	FVector RecoveryLocation;
	if (!FindRecoveryLocation(RecoveryLocation))
	{
		// Never stand up through a ceiling, another capsule, or outside the available navigation.
		GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::RequestGetUp, 0.25f, false);
		return;
	}
	const FQuat PelvisRotation = OwnerCharacter->GetMesh()->GetSocketQuaternion(Settings.PelvisBone);
	const bool bFaceUp = PelvisRotation.RotateVector(Settings.PelvisFrontAxis.GetSafeNormal()).Z > 0.0f;
	State.Phase = EFE_KnockdownPhase::GettingUp;
	State.RecoveryLocation = RecoveryLocation;
	State.RecoveryRotation = CalculateRecoveryRotation(bFaceUp);
	State.bHasRecoveryTransform = true;
	State.GetUpMontage = bFaceUp ? Settings.GetUpFaceUpMontage : Settings.GetUpFaceDownMontage;
	State.GetUpStartTime = GetServerTime();
	ApplyState();
	UAnimInstance* Anim = OwnerCharacter->GetMesh()->GetAnimInstance();
	UAnimMontage* Montage = State.GetUpMontage.Get();
	if (Anim && Montage && Anim->Montage_IsActive(Montage))
	{
		// Includes the real blend-out/play rate. Do not unlock AI based on an estimated asset duration.
		FOnMontageEnded Ended;
		Ended.BindUObject(this, &ThisClass::HandleGetUpMontageEnded);
		Anim->Montage_SetEndDelegate(Ended, Montage);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &ThisClass::RequestFinishGetUp,
			FMath::Max(0.1f, Settings.FallbackGetUpDuration), false);
	}
	OwnerCharacter->ForceNetUpdate();
}

FRotator UFE_KnockdownComponent::CalculateRecoveryRotation(bool bFaceUp) const
{
	const ACharacter* OwnerCharacter = Character.Get();
	const FVector HeadDirection = OwnerCharacter->GetMesh()->GetSocketQuaternion(Settings.PelvisBone)
		.RotateVector(Settings.PelvisUpAxis.GetSafeNormal()).GetSafeNormal2D(0.01f);
	if (HeadDirection.IsNearlyZero()) { return FRotator(0.0f, OwnerCharacter->GetActorRotation().Yaw, 0.0f); }
	// ALS get-up clips face +Y in mesh space; account for the character's mesh-to-capsule rotation.
	const FVector MeshForward = OriginalMeshTransform.TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal2D();
	const float Yaw = HeadDirection.Rotation().Yaw + (bFaceUp ? 180.0f : 0.0f) -
		MeshForward.Rotation().Yaw + Settings.GetUpYawOffset;
	return FRotator(0.0f, FRotator::NormalizeAxis(Yaw), 0.0f);
}

void UFE_KnockdownComponent::HandleGetUpMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bDeathStopped && GetOwner()->HasAuthority() && State.Phase == EFE_KnockdownPhase::GettingUp &&
		Montage == State.GetUpMontage.Get())
	{
		RequestFinishGetUp();
	}
}

void UFE_KnockdownComponent::FinishGetUp()
{
	if (!GetOwner()->HasAuthority() || State.Phase != EFE_KnockdownPhase::GettingUp) { return; }
	if (bDeathStopped || !UFE_CombatComponent::IsActorAlive(GetOwner())) { StopForDeath(); return; }
	State.Phase = EFE_KnockdownPhase::None;
	ApplyState();
	// Include CharacterMovement's floor adjustment, not the original get-up start position.
	State.RecoveryLocation = GetOwner()->GetActorLocation();
	State.RecoveryRotation = GetOwner()->GetActorRotation();
	SetKnockdownTag(false);
	GetOwner()->ForceNetUpdate();
}

void UFE_KnockdownComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	ACharacter* OwnerCharacter = Character.Get();
	if (!OwnerCharacter || !bConfigured || bDeathStopped) { return; }
	if (bGetUpRequested) { bGetUpRequested = false; BeginGetUp(); }
	if (bFinishGetUpRequested) { bFinishGetUpRequested = false; FinishGetUp(); }
	if (bPendingStateApplication) { bPendingStateApplication = false; ApplyState(); }
	RefreshTransitionTick();
	if (!OwnerCharacter->HasAuthority() || !bPresentationActive) { return; }
	// Follow like ALS: move the kinematic anchor without teleporting the simulated bodies.
	// One floor query at 10 Hz while down; the full space/nav validation runs only at recovery.
	const FVector Pelvis = OwnerCharacter->GetMesh()->GetSocketLocation(Settings.PelvisBone);
	const float HalfHeight = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Location = Pelvis;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FEKnockdownFollow), false, OwnerCharacter);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(FECollisionChannels::BuildPiece);
	FHitResult Floor;
	if (GetWorld()->LineTraceSingleByObjectType(Floor, Pelvis + FVector(0.0f, 0.0f, HalfHeight),
		Pelvis - FVector(0.0f, 0.0f, HalfHeight + 200.0f), Objects, Params) &&
		OwnerCharacter->GetCharacterMovement()->IsWalkable(Floor))
	{
		Location.Z = Floor.ImpactPoint.Z + HalfHeight + 2.0f;
	}
	OwnerCharacter->SetActorLocation(Location, false, nullptr, ETeleportType::None);
}

void UFE_KnockdownComponent::SetKnockdownTag(bool bEnabled)
{
	if (!GetOwner()->HasAuthority() || bOwnsKnockdownTag == bEnabled) { return; }
	if (bEnabled)
	{
		TaggedAbilitySystem = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
		if (UAbilitySystemComponent* ASC = TaggedAbilitySystem.Get())
		{
			ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
			bOwnsKnockdownTag = true;
		}
	}
	else
	{
		if (UAbilitySystemComponent* ASC = TaggedAbilitySystem.Get())
		{
			ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown, 1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		bOwnsKnockdownTag = false;
	}
}

float UFE_KnockdownComponent::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void UFE_KnockdownComponent::StopForDeath()
{
	bDeathStopped = true;
	bPendingStateApplication = false;
	bGetUpRequested = false;
	bFinishGetUpRequested = false;
	GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);
	SetComponentTickEnabled(false);
	RestoreMesh();
	ClearGetUpPresentation();
	SetKnockdownTag(false);
	if (GetOwner()->HasAuthority())
	{
		State = {};
		GetOwner()->ForceNetUpdate();
	}
}

void UFE_KnockdownComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);
	SetKnockdownTag(false);
	if (Character.IsValid()) { Character->GetMesh()->RemoveTickPrerequisiteComponent(this); }
	Super::EndPlay(EndPlayReason);
}
