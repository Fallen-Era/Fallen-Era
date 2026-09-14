#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FallenEraAbilitySet.generated.h"

class UFallenEraAbilitySystemComponent;
class UFallenEraGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FALLENERA_API FFallenEraAbilitySet_Ability
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Ability")
	TSubclassOf<UFallenEraGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly, Category="Ability", meta=(ClampMin="1"))
	int32 AbilityLevel = 1;

	/** The Enhanced Input binding tag placed on the granted ability spec. */
	UPROPERTY(EditDefaultsOnly, Category="Ability", meta=(Categories="Ability.Input"))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FALLENERA_API FFallenEraAbilitySet_Effect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Effect")
	TSubclassOf<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditDefaultsOnly, Category="Effect", meta=(ClampMin="0.0"))
	float EffectLevel = 1.0f;
};

/** A reusable bundle of abilities and startup effects granted by PlayerState. */
UCLASS(BlueprintType, Const)
class FALLENERA_API UFallenEraAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	void GiveToAbilitySystem(UFallenEraAbilitySystemComponent* AbilitySystemComponent, UObject* SourceObject = nullptr) const;

private:
	UPROPERTY(EditDefaultsOnly, Category="Abilities")
	TArray<FFallenEraAbilitySet_Ability> GrantedAbilities;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TArray<FFallenEraAbilitySet_Effect> GrantedEffects;
};
