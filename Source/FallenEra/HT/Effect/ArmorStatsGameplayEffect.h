#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ArmorStatsGameplayEffect.generated.h"

/** One independent persistent modifier per equipped armor slot. */
UCLASS()
class FALLENERA_API UFE_ArmorStatsGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_ArmorStatsGameplayEffect();
};
