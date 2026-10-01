#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "WeaponOffenseGameplayEffect.generated.h"

/** Infinite effect used to add the equipped weapon's Offense to AttackPower. */
UCLASS()
class FALLENERA_API UFE_WeaponOffenseGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_WeaponOffenseGameplayEffect();
};
