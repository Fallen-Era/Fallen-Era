#pragma once

#include "CoreMinimal.h"
#include "Combat/GameplayEffect/FEConditionHealthLossGameplayEffect.h"
#include "FEConditionLifetimeGameplayEffect.generated.h"

/** GAS owns the condition lifetime, periodic loss, granted tag and immunity checks. */
UCLASS()
class FALLENERA_API UFE_ConditionLifetimeGameplayEffect : public UFE_ConditionHealthLossGameplayEffect
{
	GENERATED_BODY()
public:
	UFE_ConditionLifetimeGameplayEffect();
};
