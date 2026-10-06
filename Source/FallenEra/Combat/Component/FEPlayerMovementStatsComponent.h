#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "FEPlayerMovementStatsComponent.generated.h"

enum class EFE_PlayerMovementState : uint8
{
	Idle,
	Walking,
	Sprinting
};

/**
 * Owns player walk/sprint speed and movement-state handling penalties.
 * Penalties are GAS modifiers, so traits, abilities and equipment can offset them
 * without adding movement-specific branches to attack abilities.
 */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_PlayerMovementStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFE_PlayerMovementStatsComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Immediately predicts speed locally and sends the authoritative sprint state to the server. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Movement")
	void SetSprinting(bool bNewSprinting);

	UFUNCTION(BlueprintPure, Category="FallenEra|Movement")
	bool IsSprinting() const { return bSprinting; }

	/** Re-evaluates speed and GAS state after ASC actor info is initialized/replaced. */
	void RefreshMovementState();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Speed", meta=(ClampMin="0.0", Units="cm/s"))
	float WalkSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Speed", meta=(ClampMin="0.0", Units="cm/s"))
	float SprintSpeed = 600.0f;

	/** Additive Accuracy modifier while moving without sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Handling", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float WalkingAccuracyModifier = -0.05f;

	/** Additive RecoilControl modifier while moving without sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Handling", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float WalkingRecoilControlModifier = 0.0f;

	/** Additive Accuracy modifier while sprinting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Handling", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float SprintingAccuracyModifier = -0.25f;

	/** Additive RecoilControl modifier while sprinting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Handling", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float SprintingRecoilControlModifier = -0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|State", meta=(ClampMin="0.0", Units="cm/s"))
	float MovingSpeedThreshold = 10.0f;

private:
	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_Sprinting();

	UFUNCTION()
	void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	void SetSprintingInternal(bool bNewSprinting);
	void ApplyMovementSpeed() const;
	void EvaluateMovementState();
	void ApplyMovementState(EFE_PlayerMovementState NewState);
	void ClearMovementHandlingEffect();

	UPROPERTY(ReplicatedUsing=OnRep_Sprinting, Transient)
	bool bSprinting = false;

	EFE_PlayerMovementState ActiveMovementState = EFE_PlayerMovementState::Idle;
	FActiveGameplayEffectHandle MovementHandlingEffectHandle;
};
