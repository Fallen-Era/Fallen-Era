#include "HT/AI/AISettings.h"

#include "AbilitySystem/FallenEraGameplayTags.h"

FSAIAttackMontageSettings::FSAIAttackMontageSettings()
{
	TraceStartEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.TraceStart")), false);
	TraceEndEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.TraceEnd")), false);
	AttackResetEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.Reset")), false);
}

FSAISettings::FSAISettings()
{
	FFE_ConditionApplicationChance BleedingChance;
	BleedingChance.ConditionTag = FallenEraGameplayTags::State_Condition_Bleeding;
	BleedingChance.Chance = 0.25f;
	ConditionApplicationChances.Add(BleedingChance);

	FFE_ConditionApplicationChance InfectionChance;
	InfectionChance.ConditionTag = FallenEraGameplayTags::State_Condition_Infection;
	InfectionChance.Chance = 0.1f;
	ConditionApplicationChances.Add(InfectionChance);
}
