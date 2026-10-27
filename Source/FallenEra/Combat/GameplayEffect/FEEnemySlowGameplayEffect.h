#pragma once

#include "GameplayEffect.h"
#include "FEEnemySlowGameplayEffect.generated.h"

/** Adapter for the existing State.Slowed loose-tag contract. */
UCLASS()
class FALLENERA_API UFE_EnemySlowGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UFE_EnemySlowGameplayEffect();
};
