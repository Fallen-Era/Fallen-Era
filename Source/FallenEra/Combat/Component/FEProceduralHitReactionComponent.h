#pragma once

#include "CoreMinimal.h"
#include "HitReact.h"
#include "Combat/Reaction/FEProceduralHitReactionSettings.h"
#include "FEProceduralHitReactionComponent.generated.h"

class UFE_CombatComponent;

/** Project adapter for ProcHitReact. No enemy-class dependency and no gameplay authority. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_ProceduralHitReactionComponent : public UHitReact
{
	GENERATED_BODY()

public:
	UFE_ProceduralHitReactionComponent();
	void Configure(const FFE_ProceduralHitReactionSettings& NewSettings);
	bool IsConfiguredForHits() const;
	bool IsReadyForHits() const { return IsActive() && bHasInitialized && bProfilesLoaded; }
	bool TryPlayReaction(const FFE_ProceduralHitReactionEvent& Event);
	/** Called before death collision/ragdoll takes ownership of the mesh. */
	void StopReaction();
	/** Full ragdoll temporarily owns the bodies; keep loaded profiles available for recovery. */
	void SetKnockdownSuppressed(bool bSuppressed);
	FName ResolveSimulationBone(FName HitBone, FName& OutImpulseBone) const;
	virtual bool CanHitReact_Implementation() const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void ResetHitReactSystem() override;

private:
	bool IsPresentationRelevant() const;
	void RestorePoseUpdates();
	UPROPERTY(Transient) FFE_ProceduralHitReactionSettings Settings;
	TWeakObjectPtr<UFE_CombatComponent> Combat;
	bool bConfigured = false;
	bool bKnockdownSuppressed = false;
	bool bPoseUpdatesOverridden = false;
	bool bPreviousUpdateRateOptimizations = false;
	bool bPreviousBlendPhysics = false;
};
