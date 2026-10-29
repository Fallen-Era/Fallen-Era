#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "FEArmorStatsGameplayEffect.generated.h"

/** One independent persistent modifier per equipped armor slot. */
UCLASS()
class FALLENERA_API UFE_ArmorStatsGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_ArmorStatsGameplayEffect();
};
