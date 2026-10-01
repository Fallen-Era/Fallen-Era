#include "Combat/GameplayEffect/WeaponOffenseGameplayEffect.h"

#include "Combat/CombatGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"

UFE_WeaponOffenseGameplayEffect::UFE_WeaponOffenseGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo AttackPowerModifier;
	AttackPowerModifier.Attribute = UFallenEraAttributeSet::GetAttackPowerAttribute();
	AttackPowerModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat OffenseMagnitude;
	OffenseMagnitude.DataTag = FallenEraCombatGameplayTags::SetByCaller_AttackPower;
	AttackPowerModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(OffenseMagnitude);
	Modifiers.Add(AttackPowerModifier);
}
