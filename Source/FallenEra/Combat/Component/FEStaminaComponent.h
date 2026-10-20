#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "FEStaminaComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE(FFE_OnStaminaActionRejected);

/** GAS owns the resource/tag; this component only schedules authoritative transactions. No component Tick. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_StaminaComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFE_StaminaComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void InitializeAbilitySystem();
	UFUNCTION(BlueprintPure, Category="FallenEra|Stamina")
	bool CanSpendStamina(float Amount) const;
	/** Clients only check availability; the server applies the GameplayEffect exactly once. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Stamina")
	bool TrySpendStamina(float Amount);
	/** Call on fresh action input, not on held-input retry or movement polling. */
	void BeginActionAttempt() { bFailureFeedbackSent = false; }
	void NotifyActionRejected();
	FFE_OnStaminaActionRejected OnActionRejected;
	bool CanJumpWithStamina() const { return CanSpendStamina(JumpStaminaCost); }
	void ConsumeJumpStamina();
	bool CanSprintWithStamina() const;
	void SetSprintDraining(bool bEnabled);
	void RefreshActivity();

protected:
	UPROPERTY(EditDefaultsOnly, Category="FallenEra|Stamina", meta=(ClampMin="0.0"))
	float JumpStaminaCost = 20.0f;
	UPROPERTY(EditDefaultsOnly, Category="FallenEra|Stamina", meta=(ClampMin="0.0"))
	float SprintStaminaPerSecond = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category="FallenEra|Stamina", meta=(ClampMin="0.0", Units="s"))
	float RecoveryDelay = 1.0f;
	UPROPERTY(EditDefaultsOnly, Category="FallenEra|Stamina", meta=(ClampMin="0.0"))
	float ExhaustionRecoveryThreshold = 20.0f;

private:
	void UnbindAbilitySystem();
	void HandleStaminaChanged(const FOnAttributeChangeData& Change);
	void HandleSettingsChanged(const FOnAttributeChangeData& Change);
	void HandleStateTagChanged(FGameplayTag Tag, int32 Count);
	void HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags);
	void UpdateExhaustion();
	void UpdateTimer();
	void UpdateResource();
	void ApplyStaminaChange(float Delta);
	bool IsRecoveryBlocked() const;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle StaminaHandle;
	FDelegateHandle MaxStaminaHandle;
	FDelegateHandle RecoveryRateHandle;
	FDelegateHandle AimHandle;
	FDelegateHandle DeathHandle;
	FDelegateHandle ExhaustionHandle;
	FDelegateHandle AbilityFailedHandle;
	FTimerHandle ResourceTimer;
	double RecoveryReadyTime = 0.0;
	double LastUpdateTime = 0.0;
	bool bSprintDraining = false;
	bool bFailureFeedbackSent = false;
};
