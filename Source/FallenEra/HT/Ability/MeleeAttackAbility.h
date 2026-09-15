#pragma once

#include "CoreMinimal.h"
#include "HT/Ability/PlayerAttackAbility.h"
#include "MeleeAttackAbility.generated.h"

class UFE_MeleeAttackData;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputRelease;

/** Melee attack implementation using a server-authoritative sphere sweep. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_MeleeAttackAbility : public UFE_PlayerAttackAbility
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
	void PerformCurrentAttack();
	void WaitForNextAttack();

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnAttackWindowEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnFallbackIntervalElapsed();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> AttackWindowTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> FallbackIntervalTask;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFE_MeleeAttackData>> CachedComboAttacks;

	int32 CurrentComboIndex = 0;
	int32 SequenceStep = 0;
	int32 SequenceSeed = 0;
	bool bInputReleased = false;
};
