#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/FEProjectileAttackAbility.h"
#include "FEChargedProjectileAttackAbility.generated.h"

class UAbilityTask_WaitInputRelease;
class UFE_ChargedProjectileAttackData;

/** Hold-to-charge projectile flow used by bows and throwable weapons. */
UCLASS(Abstract, Blueprintable)
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

protected:
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	void BeginRelease();
	void FireChargedProjectile();
	float CalculateChargeAlpha() const;
	float CalculateLaunchSpeed() const;
	void StartLocalChargePresentation();
	void StopLocalChargePresentation();

	/** Child-only local presentation: bow camera/reticle/AnimBP or grenade trajectory. */
	virtual void StartSpecializedChargePresentation();
	virtual void StopSpecializedChargePresentation();
	virtual bool SupportsChargedAttackData(const UFE_ChargedProjectileAttackData* AttackData) const;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;

	UPROPERTY(Transient)
	TObjectPtr<UFE_ChargedProjectileAttackData> CachedChargedAttackData;

	float ChargeStartTime = 0.0f;
	float ReleasedChargeAlpha = 0.0f;
	bool bReleaseRequested = false;
	bool bProjectileFired = false;
	bool bChargePresentationActive = false;
};
