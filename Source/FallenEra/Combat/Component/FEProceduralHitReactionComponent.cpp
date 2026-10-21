#include "Combat/Component/FEProceduralHitReactionComponent.h"

#include "Combat/Component/FECombatComponent.h"
#include "Combat/Damage/FEHitZoneMappingData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HitReactProfile.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"

UFE_ProceduralHitReactionComponent::UFE_ProceduralHitReactionComponent()
{
	bAutoActivate = false;
	bApplyHitReactOnDedicatedServer = false;
}

void UFE_ProceduralHitReactionComponent::Configure(const FFE_ProceduralHitReactionSettings& NewSettings)
{
	StopReaction();
	Settings = NewSettings;
	Combat = GetOwner()->FindComponentByClass<UFE_CombatComponent>();
	bConfigured = Settings.bEnabled && !Settings.Profile.IsNull();
	AvailableProfiles.Reset();
	if (bConfigured && GetNetMode() != NM_DedicatedServer)
	{
		AvailableProfiles.Add(Settings.Profile);
		Activate(true);
	}
}

bool UFE_ProceduralHitReactionComponent::IsConfiguredForHits() const
{
	return bConfigured;
}

bool UFE_ProceduralHitReactionComponent::IsPresentationRelevant() const
{
	if (!Mesh || !GetWorld() || GetNetMode() == NM_DedicatedServer || !Mesh->IsVisible() ||
		GetOwner()->IsHidden()) { return false; }
	// On-screen time excludes shadow-only rendering, unlike WasRecentlyRendered().
	if (Settings.bRequireRecentlyRendered &&
		GetWorld()->GetTimeSeconds() - Mesh->GetLastRenderTimeOnScreen() > 0.2f) { return false; }
	if (Settings.MaxDistance <= 0.0f) { return true; }
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!Controller || !Controller->IsLocalController()) { continue; }
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		if (FVector::DistSquared(ViewLocation, Mesh->GetComponentLocation()) <=
			FMath::Square(Settings.MaxDistance)) { return true; }
	}
	return false;
}

bool UFE_ProceduralHitReactionComponent::CanHitReact_Implementation() const
{
	return bConfigured && !bKnockdownSuppressed && UFE_CombatComponent::CanActorPlayPhysicalHitReaction(GetOwner()) && IsPresentationRelevant();
}

void UFE_ProceduralHitReactionComponent::SetKnockdownSuppressed(bool bSuppressed)
{
	if (bSuppressed)
	{
		ResetHitReactSystem();
		SetComponentTickEnabled(false);
	}
	bKnockdownSuppressed = bSuppressed;
}

FName UFE_ProceduralHitReactionComponent::ResolveSimulationBone(FName HitBone, FName& OutImpulseBone) const
{
	OutImpulseBone = NAME_None;
	const UFE_CombatComponent* OwnerCombat = Combat.Get();
	const UFE_HitZoneMappingData* Mapping = OwnerCombat ? OwnerCombat->GetLoadedHitZoneMappingData() : nullptr;
	const UPhysicsAsset* PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	const EFE_HitZone* Zone = Mapping && Mesh ? Mapping->FindHitZone(HitBone, Mesh->GetSkeletalMeshAsset()) : nullptr;
	if (!PhysicsAsset || !Zone || *Zone == EFE_HitZone::Default || *Zone == EFE_HitZone::Torso) { return NAME_None; }
	FName SimulatedBone = NAME_None;
	// Stay inside this anatomical branch. A left-arm hit can never select the spine or right arm.
	for (FName Bone = HitBone; !Bone.IsNone(); Bone = Mesh->GetParentBone(Bone))
	{
		const EFE_HitZone* ParentZone = Mapping->FindHitZone(Bone, Mesh->GetSkeletalMeshAsset());
		if (!ParentZone || *ParentZone != *Zone) { break; }
		if (PhysicsAsset->FindBodyIndex(Bone) != INDEX_NONE && !BlacklistedBones.Contains(Bone))
		{
			if (OutImpulseBone.IsNone()) { OutImpulseBone = Bone; }
			SimulatedBone = Bone;
		}
	}
	return SimulatedBone;
}

bool UFE_ProceduralHitReactionComponent::TryPlayReaction(const FFE_ProceduralHitReactionEvent& Event)
{
	if (!IsActive() || !bHasInitialized || !bProfilesLoaded || !Settings.Profile.IsValid() ||
		!CanHitReact() || (Mesh->IsSimulatingPhysics() && PhysicsBlends.IsEmpty())) { return false; }
	FName ImpulseBone;
	const FName SimulatedBone = ResolveSimulationBone(Event.HitBone, ImpulseBone);
	if (SimulatedBone.IsNone() || Event.Direction.IsNearlyZero()) { return false; }
	// Do not let a custom profile remap a safe limb root to the pelvis or another branch.
	if (const FName* Remapped = Settings.Profile.Get()->RemapSimulatedBones.Find(SimulatedBone))
	{
		if (*Remapped != SimulatedBone) { return false; }
	}
	FHitReactInputParams Params(Settings.Profile, SimulatedBone, true);
	Params.ImpulseBoneName = ImpulseBone;
	FHitReactImpulseParams Impulse;
	Impulse.LinearImpulse.bApplyImpulse = Settings.ImpulseStrength > 0.0f;
	Impulse.LinearImpulse.bFactorMass = false;
	Impulse.LinearImpulse.Impulse = Settings.ImpulseStrength;
	FHitReactImpulse_WorldParams WorldParams;
	WorldParams.LinearDirection = FVector(Event.Direction).GetSafeNormal();
	const bool bBlendPhysicsBeforeHit = Mesh->bBlendPhysics;
	const bool bApplied = HitReact(Params, Impulse, WorldParams);
	if (bApplied && !bPoseUpdatesOverridden)
	{
		bPreviousUpdateRateOptimizations = Mesh->bEnableUpdateRateOptimizations;
		bPreviousBlendPhysics = bBlendPhysicsBeforeHit;
		Mesh->bEnableUpdateRateOptimizations = false;
		bPoseUpdatesOverridden = true;
	}
	// The bundled 1.0 plugin changes collision before some rejection paths (LOD/cooldown).
	if (!bApplied && PhysicsBlends.IsEmpty()) { ResetHitReactSystem(); }
	// A cooldown-throttled repeat hit must not inject a montage over this limb's ongoing physics.
	return bApplied || PhysicsBlends.ContainsByPredicate([SimulatedBone](const FHitReactPhysics& Blend)
	{
		return Blend.SimulatedBoneName == SimulatedBone;
	});
}

void UFE_ProceduralHitReactionComponent::RestorePoseUpdates()
{
	if (Mesh && bPoseUpdatesOverridden)
	{
		Mesh->bEnableUpdateRateOptimizations = bPreviousUpdateRateOptimizations;
		Mesh->bBlendPhysics = bPreviousBlendPhysics;
	}
	bPoseUpdatesOverridden = false;
}

void UFE_ProceduralHitReactionComponent::ResetHitReactSystem()
{
	Super::ResetHitReactSystem();
	PendingImpulse = {};
	SmoothedBoneWeights.Reset();
	if (Mesh)
	{
		if (bCollisionEnabledChanged) { Mesh->SetCollisionEnabled(DefaultCollisionEnabled); }
		if (bConstraintProfileChanged) { Mesh->SetConstraintProfileForAll(NAME_None); }
		if (bPhysicalAnimationProfileChanged && PhysicalAnimation)
		{
			PhysicalAnimation->ApplyPhysicalAnimationProfileBelow(NAME_None, NAME_None, false);
		}
	}
	bCollisionEnabledChanged = bConstraintProfileChanged = bPhysicalAnimationProfileChanged = false;
	RestorePoseUpdates();
}

void UFE_ProceduralHitReactionComponent::StopReaction()
{
	bConfigured = false;
	CancelAsyncLoading();
	Deactivate();
	SetComponentTickEnabled(false);
}

void UFE_ProceduralHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (PhysicsBlends.IsEmpty()) { RestorePoseUpdates(); }
}

void UFE_ProceduralHitReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bConfigured = false;
	StopReaction();
	Super::EndPlay(EndPlayReason);
}
