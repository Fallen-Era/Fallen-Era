#include "HT/Effect/ArmorStatsGameplayEffect.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "HT/Armor/ArmorGameplayTags.h"

UFE_ArmorStatsGameplayEffect::UFE_ArmorStatsGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	// UGameplayEffect defaults to None: each slot keeps an independent active effect.

	FGameplayModifierInfo Defense;
	Defense.Attribute = UFallenEraAttributeSet::GetDefensePowerAttribute();
	Defense.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat DefenseMagnitude;
	DefenseMagnitude.DataTag = FE_ArmorGameplayTags::SetByCaller_DefensePower;
	Defense.ModifierMagnitude = FGameplayEffectModifierMagnitude(DefenseMagnitude);
	Modifiers.Add(Defense);

	FGameplayModifierInfo Resistance;
	Resistance.Attribute = UFallenEraAttributeSet::GetKnockbackResistanceAttribute();
	Resistance.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat ResistanceMagnitude;
	ResistanceMagnitude.DataTag = FE_ArmorGameplayTags::SetByCaller_KnockbackResistance;
	Resistance.ModifierMagnitude = FGameplayEffectModifierMagnitude(ResistanceMagnitude);
	Modifiers.Add(Resistance);
}
