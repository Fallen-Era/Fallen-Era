#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"
#include "Combat/AI/FEAISettings.h"
#include "FEEnemyMeleeAttackAbility.generated.h"

class AFE_EnemyCharacter;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UFE_MeleeTraceTask;

/** Server-only enemy melee attack. Tasks only exist for the active attack and hit window. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_EnemyMeleeAttackAbility : public UFallenEraGameplayAbility
{
	GENERATED_BODY()

public:
	UFE_EnemyMeleeAttackAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

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
	void HandleTraceStart(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTraceEnd(FGameplayEventData Payload);

	UFUNCTION()
	void HandleAttackReset(FGameplayEventData Payload);

	UFUNCTION()
	void HandleAttackTimeout();

	UFUNCTION()
	void ExecuteTrace();

	void StartTrace();
	void StopTrace();
	void FinishAttack(bool bWasCancelled = false);
	const FSAIAttackMontageSettings* SelectAttackSettings(
		const AFE_EnemyCharacter& EnemyCharacter) const;

	UPROPERTY(Transient)
	TObjectPtr<AFE_EnemyCharacter> CachedEnemyCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	FSAIAttackMontageSettings ActiveAttackSettings;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceStartTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> AttackResetTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> AttackTimeoutTask;

	UPROPERTY(Transient)
	TObjectPtr<UFE_MeleeTraceTask> TraceTickTask;

	TSet<TWeakObjectPtr<AActor>> DamagedActors;
	float NextActivationAllowedTime = 0.0f;
	bool bTraceActive = false;
};
