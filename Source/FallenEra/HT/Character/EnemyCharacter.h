#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "HT/Interface/Damageable.h"
#include "EnemyCharacter.generated.h"

class UFE_CombatComponent;
class UFE_EquipmentComponent;
class UFallenEraAbilitySet;
class UFallenEraAbilitySystemComponent;
class UFallenEraAttributeSet;

/** Reusable ACharacter base for enemies with their own GAS owner and combat component. */
UCLASS(Abstract)
class FALLENERA_API AFE_EnemyCharacter : public ACharacter, public IAbilitySystemInterface, public IFE_Damageable
{
	GENERATED_BODY()

public:
	AFE_EnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FFE_CombatDamageResult ReceiveCombatDamage_Implementation(
		const FFE_CombatDamageRequest& DamageRequest) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	UFallenEraAbilitySystemComponent* GetEnemyAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	const UFallenEraAttributeSet* GetEnemyAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	UFE_CombatComponent* GetCombatComponent() const { return CombatComponent; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	UFE_EquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Equipment")
	TObjectPtr<UFE_EquipmentComponent> EquipmentComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Abilities")
	TArray<TObjectPtr<UFallenEraAbilitySet>> DefaultAbilitySets;

private:
	void InitializeEnemyAbilitySystem();
};
