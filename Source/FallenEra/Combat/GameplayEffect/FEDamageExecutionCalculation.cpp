#include "Combat/GameplayEffect/FEDamageExecutionCalculation.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"

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
	// [Server Only] Clients observe replicated Health, never roll their own damage.
	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative())
	{
		return;
	}

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

	const float RequestedMultiplier = Spec.GetSetByCallerMagnitude(
		FallenEraCombatGameplayTags::SetByCaller_Damage_HitRegionMultiplier, false, 1.0f);
	// Two uniform samples form a triangular distribution: ordinary hits are more common than extremes.
	const float CenteredRoll = DamageVarianceRatio > 0.0f ? FMath::FRand() + FMath::FRand() - 1.0f : 0.0f;
	const float FinalDamage = CalculateDamage(SourceAttackPower, TargetDefensePower, RequestedMultiplier, CenteredRoll);

	if (FinalDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UFallenEraAttributeSet::GetHealthAttribute(),
			EGameplayModOp::Additive,
			-FinalDamage));
	}
}

float UFE_DamageExecutionCalculation::CalculateDamage(
	float AttackPower, float DefensePower, float HitRegionMultiplier, float CenteredRoll) const
{
	if (!FMath::IsFinite(AttackPower) || !FMath::IsFinite(DefensePower))
	{
		return 0.0f;
	}
	const float Variance = FMath::IsFinite(DamageVarianceRatio) ? FMath::Clamp(DamageVarianceRatio, 0.0f, 1.0f) : 0.0f;
	const float Roll = FMath::IsFinite(CenteredRoll) ? FMath::Clamp(CenteredRoll, -1.0f, 1.0f) : 0.0f;
	const float RegionMultiplier = FMath::IsFinite(HitRegionMultiplier) ? FMath::Max(0.0f, HitRegionMultiplier) : 1.0f;
	const float RolledAttackPower = FMath::Max(AttackPower, 0.0f) * (1.0f + Variance * Roll);
	// Defense still precedes the hit-region multiplier; Area hits supply x1.
	const float Damage = FMath::Max(RolledAttackPower - FMath::Max(DefensePower, 0.0f), 0.0f) * RegionMultiplier;
	return FMath::IsFinite(Damage) ? Damage : 0.0f;
}
