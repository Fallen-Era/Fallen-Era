#include "HT/Effect/DamageExecutionCalculation.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "GameplayEffect.h"

namespace
{
	struct FFE_DamageStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
		DECLARE_ATTRIBUTE_CAPTUREDEF(DefensePower);

		FFE_DamageStatics()
		{
			DEFINE_ATTRIBUTE_CAPTUREDEF(UFallenEraAttributeSet, AttackPower, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UFallenEraAttributeSet, DefensePower, Target, false);
		}
	};

	const FFE_DamageStatics& DamageStatics()
	{
		static FFE_DamageStatics Statics;
		return Statics;
	}
}

UFE_DamageExecutionCalculation::UFE_DamageExecutionCalculation()
{
	// Captures must be registered here or the GameplayEffectSpec will not
	// populate source/target magnitudes and execution will read zero values.
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().DefensePowerDef);
}

void UFE_DamageExecutionCalculation::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float SourceAttackPower = 0.0f;
	const bool bCapturedAttackPower = ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().AttackPowerDef,
		EvaluationParameters,
		SourceAttackPower);

	float TargetDefensePower = 0.0f;
	const bool bCapturedDefensePower = ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().DefensePowerDef,
		EvaluationParameters,
		TargetDefensePower);

	if (!ensureMsgf(
		bCapturedAttackPower && bCapturedDefensePower,
		TEXT("Damage execution could not capture AttackPower or DefensePower. Check RelevantAttributesToCapture and the source/target AttributeSets.")))
	{
		return;
	}

	const float FinalDamage = FMath::Max(
		FMath::Max(SourceAttackPower, 0.0f) - FMath::Max(TargetDefensePower, 0.0f),
		0.0f);

	if (FinalDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UFallenEraAttributeSet::GetHealthAttribute(),
			EGameplayModOp::Additive,
			-FinalDamage));
	}
}
