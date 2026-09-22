#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DamageGameplayEffect.generated.h"

/** Instant damage effect executed by UFE_DamageExecutionCalculation. */
UCLASS()
class FALLENERA_API UFE_DamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_DamageGameplayEffect();

	/**
	 * Re-applies native defaults to existing Blueprint-derived assets.  Native
	 * constructor defaults are not serialized into an already-saved BP CDO, so
	 * this keeps BP_DamageEffect executable after the calculation was migrated
	 * from a modifier to an execution calculation.
	 */
	virtual void PostLoad() override;
};
