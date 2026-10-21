#pragma once

#include "GameplayEffect.h"
#include "FEStaminaChangeGameplayEffect.generated.h"

/** Shared authoritative resource transaction for action costs and automatic recovery. */
UCLASS()
class FALLENERA_API UFE_StaminaChangeGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UFE_StaminaChangeGameplayEffect();
};
