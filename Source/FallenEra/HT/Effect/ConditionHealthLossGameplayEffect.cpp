#include "HT/Effect/ConditionHealthLossGameplayEffect.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystem/FallenEraGameplayTags.h"

UFE_ConditionHealthLossGameplayEffect::UFE_ConditionHealthLossGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = UFallenEraAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat HealthLossMagnitude;
	HealthLossMagnitude.DataTag = FallenEraGameplayTags::SetByCaller_Condition_HealthLoss;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealthLossMagnitude);
	Modifiers.Add(HealthModifier);
}
