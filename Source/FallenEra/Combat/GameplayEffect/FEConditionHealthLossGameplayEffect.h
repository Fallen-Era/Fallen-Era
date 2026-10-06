#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "FEConditionHealthLossGameplayEffect.generated.h"

/** Instant fixed health loss used by timed conditions without Attack/Defense calculation. */
UCLASS()
class FALLENERA_API UFE_ConditionHealthLossGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_ConditionHealthLossGameplayEffect();
};
