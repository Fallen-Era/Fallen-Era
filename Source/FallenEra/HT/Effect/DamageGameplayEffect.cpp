#include "HT/Effect/DamageGameplayEffect.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "HT/Effect/DamageExecutionCalculation.h"

UFE_DamageGameplayEffect::UFE_DamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UFE_DamageExecutionCalculation::StaticClass();
	Executions.Add(DamageExecution);

	FGameplayEffectCue ImpactCue;
	ImpactCue.GameplayCueTags.AddTag(FallenEraGameplayTags::GameplayCue_Impact);
	GameplayCues.Add(ImpactCue);
}

void UFE_DamageGameplayEffect::PostLoad()
{
	Super::PostLoad();
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Existing Blueprint children keep their serialized CDO properties when a
	// native parent changes. Ensure the execution is present for those assets as
	// well as for newly created native/Blueprint effects.
	bool bHasDamageExecution = false;
	for (int32 ExecutionIndex = Executions.Num() - 1; ExecutionIndex >= 0; --ExecutionIndex)
	{
		if (Executions[ExecutionIndex].CalculationClass != UFE_DamageExecutionCalculation::StaticClass())
		{
			continue;
		}

		// A Blueprint can retain the native entry and also serialize a manually
		// added one. Keep exactly one; duplicate executions would apply 5 damage
		// twice and appear as 10 damage against AttackPower 10 / DefensePower 5.
		if (bHasDamageExecution)
		{
			Executions.RemoveAt(ExecutionIndex);
		}
		else
		{
			bHasDamageExecution = true;
		}
	}

	if (!bHasDamageExecution)
	{
		FGameplayEffectExecutionDefinition DamageExecution;
		DamageExecution.CalculationClass = UFE_DamageExecutionCalculation::StaticClass();
		Executions.Add(DamageExecution);
	}
}
