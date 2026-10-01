#include "Combat/GameplayEffect/FEConditionHealthLossGameplayEffect.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"

UFE_ConditionHealthLossGameplayEffect::UFE_ConditionHealthLossGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = UFallenEraAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat HealthLossMagnitude;
	HealthLossMagnitude.DataTag = FallenEraCombatGameplayTags::SetByCaller_Condition_HealthLoss;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealthLossMagnitude);
	Modifiers.Add(HealthModifier);
}
