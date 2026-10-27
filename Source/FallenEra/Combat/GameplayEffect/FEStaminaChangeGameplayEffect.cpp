#include "Combat/GameplayEffect/FEStaminaChangeGameplayEffect.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"

UFE_StaminaChangeGameplayEffect::UFE_StaminaChangeGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UFallenEraAttributeSet::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = FallenEraCombatGameplayTags::SetByCaller_Stamina_Change;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(Modifier);
}
