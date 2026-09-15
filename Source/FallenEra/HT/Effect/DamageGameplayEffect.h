#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DamageGameplayEffect.generated.h"

/** Instant damage effect. The amount is supplied with SetByCaller.Damage. */
UCLASS()
class FALLENERA_API UFE_DamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_DamageGameplayEffect();
};
