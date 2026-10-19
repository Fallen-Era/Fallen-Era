#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/FEPlayerAttackAbility.h"
#include "FEMeleeAttackAbility.generated.h"

class UFE_MeleeAttackData;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputRelease;
class AActor;

/** One server-authoritative center/box query per swing, triggered by an attack notify. */
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

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	virtual ECollisionChannel GetAttackTraceChannel() const override;
	virtual void ExecuteAttack(const ACharacter* CombatCharacter, const UFE_WeaponItemData* WeaponData, const UFE_WeaponAttackData* AttackData) const override;

private:
	void PerformCurrentAttack();
	void WaitForAttackEvents();
	void CompleteCurrentAttack();
	int32 SelectDeterministicRandomAttackIndex();

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnAttackStartEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackEndEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackResetEvent(FGameplayEventData Payload);

	void ExecuteCurrentTrace();

	UFUNCTION()
	void OnFallbackIntervalElapsed();
	float GetAttackResetDelay(const UFE_MeleeAttackData* AttackData) const;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> AttackWindowTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> AttackStartTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> AttackResetTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> FallbackIntervalTask;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFE_MeleeAttackData>> CachedComboAttacks;

	int32 CurrentComboIndex = 0;
	uint32 AttackSelectionSeed = 0;
	int32 AttackSelectionOrdinal = 0;
	bool bAttackAutomatic = false;
	bool bAttackInProgress = false;
	bool bAttackTraceConsumed = false;
	bool bInputReleased = false;

	struct FFE_MeleeHit
	{
		FHitResult Hit;
		float QualityMultiplier = 1.0f;
	};
	/** First contact locks region and quality, so later camera motion cannot upgrade/re-hit the target. */
	mutable TMap<TWeakObjectPtr<AActor>, FFE_MeleeHit> FirstHitsThisAttack;
	mutable int32 AreaTargetsHit = 0;
	/** World objects are center-ray only, and at most one is selected per swing. */
	mutable TWeakObjectPtr<AActor> WorldTargetThisAttack;
	mutable bool bWorldTargetSelected = false;
};
