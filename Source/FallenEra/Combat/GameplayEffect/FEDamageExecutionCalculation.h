#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "FEDamageExecutionCalculation.generated.h"

/**
 * Rolls proportional, center-weighted damage from equipped/source AttackPower,
 * reduced by the target DefensePower, then multiplied by the receiver's hit-region setting.
 */
UCLASS()
class FALLENERA_API UFE_DamageExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UFE_DamageExecutionCalculation();

	/** 0 disables variance; 0.1 rolls attack power between 90% and 110%. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Damage", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DamageVarianceRatio = 0.1f;

	/** Pure calculation. CenteredRoll is in [-1, 1], with zero representing average attack power. */
	float CalculateDamage(float AttackPower, float DefensePower, float HitRegionMultiplier, float CenteredRoll) const;

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
