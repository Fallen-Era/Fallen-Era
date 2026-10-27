#include "Combat/GameplayEffect/FEWeaponOffenseGameplayEffect.h"

#include "Combat/FECombatGameplayTags.h"
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

	FGameplayModifierInfo AccuracyModifier;
	AccuracyModifier.Attribute = UFallenEraAttributeSet::GetAccuracyAttribute();
	AccuracyModifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat AccuracyMagnitude;
	AccuracyMagnitude.DataTag = FallenEraCombatGameplayTags::SetByCaller_Accuracy;
	AccuracyModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(AccuracyMagnitude);
	Modifiers.Add(AccuracyModifier);

	FGameplayModifierInfo RecoilControlModifier;
	RecoilControlModifier.Attribute = UFallenEraAttributeSet::GetRecoilControlAttribute();
	RecoilControlModifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat RecoilMagnitude;
	RecoilMagnitude.DataTag = FallenEraCombatGameplayTags::SetByCaller_RecoilControl;
	RecoilControlModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(RecoilMagnitude);
	Modifiers.Add(RecoilControlModifier);
}
