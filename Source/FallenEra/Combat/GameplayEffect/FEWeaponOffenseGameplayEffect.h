#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "FEWeaponOffenseGameplayEffect.generated.h"

/** Infinite effect used to apply equipped weapon offense and normalized handling stats. */
UCLASS()
class FALLENERA_API UFE_WeaponOffenseGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_WeaponOffenseGameplayEffect();
};
