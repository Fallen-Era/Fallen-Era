#include "Combat/GameplayEffect/FEArmorStatsGameplayEffect.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"

UFE_ArmorStatsGameplayEffect::UFE_ArmorStatsGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	// UGameplayEffect defaults to None: each slot keeps an independent active effect.

	FGameplayModifierInfo Defense;
	Defense.Attribute = UFallenEraAttributeSet::GetDefensePowerAttribute();
	Defense.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat DefenseMagnitude;
	DefenseMagnitude.DataTag = FallenEraCombatGameplayTags::SetByCaller_DefensePower;
	Defense.ModifierMagnitude = FGameplayEffectModifierMagnitude(DefenseMagnitude);
	Modifiers.Add(Defense);

	FGameplayModifierInfo Resistance;
	Resistance.Attribute = UFallenEraAttributeSet::GetKnockbackResistanceAttribute();
	Resistance.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat ResistanceMagnitude;
	ResistanceMagnitude.DataTag = FallenEraCombatGameplayTags::SetByCaller_KnockbackResistance;
	Resistance.ModifierMagnitude = FGameplayEffectModifierMagnitude(ResistanceMagnitude);
	Modifiers.Add(Resistance);
}
