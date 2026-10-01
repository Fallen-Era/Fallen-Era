#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "HT/Condition/CharacterConditionTypes.h"
#include "CharacterStatusComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnFEActiveConditionsChanged);

/**
 * Server-authoritative owner of timed character buffs/debuffs.
 * Only compact active condition tags and end times are replicated for the owning player's UI.
 */
UCLASS(ClassGroup=(FallenEra), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_CharacterStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFE_CharacterStatusComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Rolls the condition probabilities supplied by the damage source. Must run on the authority. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Status")
	void TryApplyConditionsFromDamageSource(AActor* DamageSource);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Status")
	bool ApplyCondition(FGameplayTag ConditionTag);

	/** Treatment and cleansing use this same removal path. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Status")
	bool RemoveCondition(FGameplayTag ConditionTag);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Status")
	void RemoveAllConditions();

	UFUNCTION(BlueprintPure, Category="FallenEra|Status")
	bool HasCondition(FGameplayTag ConditionTag) const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Status")
	TArray<FGameplayTag> GetActiveConditionTags() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Status")
	TArray<FFE_ActiveCharacterCondition> GetActiveConditions() const { return ActiveConditions; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Status")
	bool GetConditionDefinition(FGameplayTag ConditionTag, FFE_CharacterConditionDefinition& OutDefinition) const;

	FOnFEActiveConditionsChanged& OnActiveConditionsChanged() { return ActiveConditionsChanged; }

protected:
	/** Initial bleeding behavior: 5 HP every 2 seconds for 10 seconds. Application chance belongs to the attacker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Status|Bleeding")
	FFE_CharacterConditionDefinition BleedingCondition;

	/** Initial infection behavior: death if untreated for 60 seconds. Application chance belongs to the attacker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Status|Infection")
	FFE_CharacterConditionDefinition InfectionCondition;

	/** Fracture, buffs, and future conditions can be added here without changing the component. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Status")
	TArray<FFE_CharacterConditionDefinition> AdditionalConditions;

	UPROPERTY(ReplicatedUsing=OnRep_ActiveConditions)
	TArray<FFE_ActiveCharacterCondition> ActiveConditions;

private:
	struct FActiveConditionRuntime
	{
		FTimerHandle TickTimer;
		FTimerHandle ExpirationTimer;
		FActiveGameplayEffectHandle AppliedEffectHandle;
	};

	UFUNCTION()
	void OnRep_ActiveConditions();

	const FFE_CharacterConditionDefinition* FindConditionDefinition(FGameplayTag ConditionTag) const;
	void StartCondition(const FFE_CharacterConditionDefinition& Definition);
	void HandleConditionTick(FGameplayTag ConditionTag);
	void HandleConditionExpired(FGameplayTag ConditionTag);
	void ApplyFixedHealthLoss(float HealthLoss);
	void NotifyConditionsChanged();
	double GetServerWorldTime() const;

	TMap<FGameplayTag, FActiveConditionRuntime> ActiveConditionRuntime;
	FOnFEActiveConditionsChanged ActiveConditionsChanged;
};
