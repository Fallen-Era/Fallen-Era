#pragma once

#include "CoreMinimal.h"
#include "HT/Ability/PlayerAttackAbility.h"
#include "ProjectileAttackAbility.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitInputRelease;
class UFE_ProjectileAttackData;

/** Projectile attack implementation. The projectile owns its later impact/damage response. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_ProjectileAttackAbility : public UFE_PlayerAttackAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void ExecuteAttack(const AFE_CombatCharacter* CombatCharacter, const UFE_WeaponItemData* WeaponData, const UFE_WeaponAttackData* AttackData) const override;

private:
	void FireOnce();
	void ScheduleNextShot();

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnFireIntervalElapsed();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> FireDelayTask;

	UPROPERTY(Transient)
	TObjectPtr<UFE_ProjectileAttackData> CachedProjectileAttackData;

	UPROPERTY(Transient)
	TSubclassOf<AActor> CachedProjectileClass;

	bool bInputReleased = false;
};
