#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetSerialization.h"
#include "Combat/Reaction/FEKnockdownSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FEKnockdownComponent.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class USkeletalMeshComponent;
class UAnimMontage;

UENUM(BlueprintType)
enum class EFE_KnockdownPhase : uint8
{
	None,
	Ragdoll,
	GettingUp
};

/** Persistent transitions allow a newly relevant client to restore the current state. */
USTRUCT()
struct FFE_KnockdownState
{
	GENERATED_BODY()
	UPROPERTY() EFE_KnockdownPhase Phase = EFE_KnockdownPhase::None;
	UPROPERTY() FName HitBone;
	UPROPERTY() FVector_NetQuantizeNormal Direction = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize RecoveryLocation = FVector::ZeroVector;
	UPROPERTY() FRotator RecoveryRotation = FRotator::ZeroRotator;
	UPROPERTY() TSoftObjectPtr<UAnimMontage> GetUpMontage;
	UPROPERTY() float GetUpStartTime = 0.0f;
	UPROPERTY() bool bHasRecoveryTransform = false;
};

/** Server-owned temporary full ragdoll. No dependency on a specific enemy character/controller. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_KnockdownComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFE_KnockdownComponent();
	void Configure(const FFE_KnockdownSettings& NewSettings);
	/** Single entry point for future accumulated leg-damage thresholds as well. */
	bool TryStartFromHit(FName HitBone, const FVector& Direction);
	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Knockdown")
	bool IsKnockedDown() const { return State.Phase != EFE_KnockdownPhase::None || bPresentationActive || bGetUpPresentationActive; }
	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Knockdown")
	EFE_KnockdownPhase GetPhase() const { return State.Phase; }
	/** Called before death collision/montage setup; never restores a dead actor's movement. */
	void StopForDeath();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION() void OnRep_State();
	void ApplyState();
	void QueueStateApplication();
	void RefreshTransitionTick();
	void EnterRagdoll();
	void RestoreMesh();
	void ClearGetUpPresentation();
	void BeginGetUp();
	void FinishGetUp();
	void RequestGetUp();
	void RequestFinishGetUp();
	void HandleGetUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	FRotator CalculateRecoveryRotation(bool bFaceUp) const;
	bool FindRecoveryLocation(FVector& OutLocation) const;
	void SetKnockdownTag(bool bEnabled);
	float GetServerTime() const;

	UPROPERTY(ReplicatedUsing=OnRep_State) FFE_KnockdownState State;
	UPROPERTY(Transient) FFE_KnockdownSettings Settings;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UAbilitySystemComponent> TaggedAbilitySystem;
	FTransform OriginalMeshTransform;
	FCollisionResponseContainer OriginalMeshResponses;
	ECollisionEnabled::Type OriginalMeshCollision = ECollisionEnabled::QueryOnly;
	ECollisionEnabled::Type OriginalCapsuleCollision = ECollisionEnabled::QueryAndPhysics;
	EVisibilityBasedAnimTickOption OriginalAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	EMovementMode OriginalMovementMode = MOVE_Walking;
	uint8 OriginalCustomMovementMode = 0;
	bool bOriginalURO = false;
	bool bOriginalBlendPhysics = false;
	bool bPresentationActive = false;
	bool bConfigured = false;
	bool bOwnsKnockdownTag = false;
	bool bDeathStopped = false;
	bool bGetUpPresentationActive = false;
	bool bPendingStateApplication = false;
	bool bGetUpRequested = false;
	bool bFinishGetUpRequested = false;
	FTimerHandle PhaseTimer;
	EFE_KnockdownPhase AppliedPhase = EFE_KnockdownPhase::None;
};
