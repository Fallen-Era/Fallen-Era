#include "Combat/GameplayEffect/FEMovementHandlingGameplayEffect.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"

UFE_MovementHandlingGameplayEffect::UFE_MovementHandlingGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

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
