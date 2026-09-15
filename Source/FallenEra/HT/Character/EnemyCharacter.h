#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "EnemyCharacter.generated.h"

class UFE_CombatComponent;
class UFallenEraAbilitySet;
class UFallenEraAbilitySystemComponent;
class UFallenEraAttributeSet;

/** Reusable ACharacter base for enemies with their own GAS owner and combat component. */
UCLASS(Abstract)
class FALLENERA_API AFE_EnemyCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFE_EnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	UFallenEraAbilitySystemComponent* GetEnemyAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	const UFallenEraAttributeSet* GetEnemyAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	UFE_CombatComponent* GetCombatComponent() const { return CombatComponent; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	bool IsDead() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Abilities")
	TObjectPtr<UFallenEraAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Abilities")
	TObjectPtr<UFallenEraAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Combat")
	TObjectPtr<UFE_CombatComponent> CombatComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Abilities")
	TArray<TObjectPtr<UFallenEraAbilitySet>> DefaultAbilitySets;

private:
	void InitializeEnemyAbilitySystem();
};
