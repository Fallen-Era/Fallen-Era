#include "HT/Effect/WeaponOffenseGameplayEffect.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"

UFE_WeaponOffenseGameplayEffect::UFE_WeaponOffenseGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo AttackPowerModifier;
	AttackPowerModifier.Attribute = UFallenEraAttributeSet::GetAttackPowerAttribute();
	AttackPowerModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat OffenseMagnitude;
	OffenseMagnitude.DataTag = FallenEraGameplayTags::SetByCaller_AttackPower;
	AttackPowerModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(OffenseMagnitude);
	Modifiers.Add(AttackPowerModifier);
}
