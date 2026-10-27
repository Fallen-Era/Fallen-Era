#include "Combat/GameplayEffect/FEEnemySlowGameplayEffect.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"

UFE_EnemySlowGameplayEffect::UFE_EnemySlowGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UFallenEraAttributeSet::GetMovementSpeedMultiplierAttribute();
	Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
	Modifier.ModifierMagnitude = FScalableFloat(0.5f);
}
