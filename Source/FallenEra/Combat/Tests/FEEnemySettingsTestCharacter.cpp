#include "Combat/Tests/FEEnemySettingsTestCharacter.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/AI/FEAISettingsDataAsset.h"

AFE_EnemySettingsTestCharacter::AFE_EnemySettingsTestCharacter()
{
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AFE_EnemySettingsTestCharacter::ConfigurePreset(UFE_AISettingsDataAsset* Preset)
{
	AISettingsDataAssets = { Preset };
	SelectedAISettingsIndex = 0;
}

UFE_EnemySettingsTestEffect::UFE_EnemySettingsTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UFallenEraAttributeSet::GetAttackPowerAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(7.0f);
}
