#pragma once

#include "CoreMinimal.h"
#include "HT/Ability/PlayerAttackAbility.h"
#include "MeleeAttackAbility.generated.h"

class UFE_MeleeAttackData;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputRelease;
class UFE_MeleeTraceTask;
class AActor;

/** Melee attack implementation using a server-authoritative socket sweep. */
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
	virtual void ExecuteAttack(const ACharacter* CombatCharacter, const UFE_WeaponItemData* WeaponData, const UFE_WeaponAttackData* AttackData) const override;

private:
	void PerformCurrentAttack();
	void StartMeleeTrace();
	void StopMeleeTrace();
	void WaitForAttackEvents();
	void CompleteCurrentAttack();

	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnAttackStartEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackEndEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackResetEvent(FGameplayEventData Payload);

	UFUNCTION()
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
	TObjectPtr<UFE_MeleeTraceTask> MeleeTraceTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> FallbackIntervalTask;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFE_MeleeAttackData>> CachedComboAttacks;

	int32 CurrentComboIndex = 0;
	bool bAttackAutomatic = false;
	bool bAttackInProgress = false;
	bool bMeleeTraceActive = false;
	bool bInputReleased = false;

	/** Prevents applying the same melee attack to the same actor every trace frame. */
	mutable TSet<AActor*> DamagedActorsThisAttack;
	mutable TMap<TWeakObjectPtr<AActor>, float> NextRepeatedHitTimes;
};
