#pragma once

#include "CoreMinimal.h"
#include "HT/Ability/ProjectileAttackAbility.h"
#include "ChargedProjectileAttackAbility.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitInputRelease;
class UFE_ChargedProjectileAttackData;
class UNiagaraComponent;

/** Hold-to-charge projectile flow used by bows and throwable weapons. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_ChargedProjectileAttackAbility : public UFE_ProjectileAttackAbility
{
	GENERATED_BODY()

public:
	UFE_ChargedProjectileAttackAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	UFUNCTION()
	void HandleTrajectoryUpdate();

	void BeginRelease();
	void FireChargedProjectile();
	float CalculateChargeAlpha() const;
	float CalculateLaunchSpeed() const;
	void StartLocalChargePresentation();
	void StopLocalChargePresentation();
	void UpdateLocalTrajectory();
	void ScheduleTrajectoryUpdate();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> TrajectoryUpdateTask;

	UPROPERTY(Transient)
	TObjectPtr<UFE_ChargedProjectileAttackData> CachedChargedAttackData;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TrajectoryComponent;

	float ChargeStartTime = 0.0f;
	float ReleasedChargeAlpha = 0.0f;
	bool bReleaseRequested = false;
	bool bProjectileFired = false;
	bool bChargePresentationActive = false;
};
