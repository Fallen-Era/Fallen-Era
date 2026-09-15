#pragma once

#include "CoreMinimal.h"
#include "HT/Ability/PlayerAttackAbility.h"
#include "HitscanAttackAbility.generated.h"

class UAbilityTask_WaitDelay;
class UAbilityTask_WaitInputRelease;
class UFE_HitscanAttackData;

/** Hitscan attack that supports one-shot and hold-to-fire automatic modes. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_HitscanAttackAbility : public UFE_PlayerAttackAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

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

	bool bInputReleased = false;
};
