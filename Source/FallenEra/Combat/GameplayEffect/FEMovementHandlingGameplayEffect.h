#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "FEMovementHandlingGameplayEffect.generated.h"

/** Infinite movement-state penalty applied to Accuracy and RecoilControl. */
UCLASS()
class FALLENERA_API UFE_MovementHandlingGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_MovementHandlingGameplayEffect();
};
