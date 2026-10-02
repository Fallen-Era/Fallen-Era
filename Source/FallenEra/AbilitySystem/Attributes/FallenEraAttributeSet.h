#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "FallenEraAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Core combat attributes used by the damage execution calculation. */
UCLASS()
class FALLENERA_API UFallenEraAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UFallenEraAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Attributes|Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UFallenEraAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Attributes|Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UFallenEraAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Stamina, Category="Attributes|Stamina")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UFallenEraAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxStamina, Category="Attributes|Stamina")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UFallenEraAttributeSet, MaxStamina)

	/** Persistent attack power, including the currently equipped weapon's Offense bonus. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackPower, Category="Attributes|Combat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UFallenEraAttributeSet, AttackPower)

	/** Persistent defense value captured from the damage target. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DefensePower, Category="Attributes|Combat")
	FGameplayAttributeData DefensePower;
	ATTRIBUTE_ACCESSORS(UFallenEraAttributeSet, DefensePower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_KnockbackResistance, Category="Attributes|Combat")
	FGameplayAttributeData KnockbackResistance;
	ATTRIBUTE_ACCESSORS(UFallenEraAttributeSet, KnockbackResistance)

	UPROPERTY(BlueprintReadOnly, Category="Attributes|Meta")
	FGameplayAttributeData Healing;
	ATTRIBUTE_ACCESSORS(UFallenEraAttributeSet, Healing)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_DefensePower(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_KnockbackResistance(const FGameplayAttributeData& OldValue);

private:
	void UpdateDeadStateTag();
};
