#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"

#include "Combat/FECombatGameplayTags.h"
#include "Combat/GameplayEffect/FEDamageExecutionCalculation.h"

UFE_DamageGameplayEffect::UFE_DamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UFE_DamageExecutionCalculation::StaticClass();
	Executions.Add(DamageExecution);

	FGameplayEffectCue ImpactCue;
	ImpactCue.GameplayCueTags.AddTag(FallenEraCombatGameplayTags::GameplayCue_Impact);
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
		const UClass* CalculationClass = Executions[ExecutionIndex].CalculationClass;
		if (!CalculationClass || !CalculationClass->IsChildOf(UFE_DamageExecutionCalculation::StaticClass()))
		{
			continue;
		}

		// A Blueprint can retain the native entry and also serialize a manually
		// added one. Keep the last configured entry, including a Blueprint child
		// with its own variance settings; duplicate executions would deal damage twice.
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
