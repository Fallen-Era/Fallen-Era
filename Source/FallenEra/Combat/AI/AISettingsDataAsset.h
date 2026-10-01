#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/AI/AISettings.h"
#include "AISettingsDataAsset.generated.h"

/** Data asset used to share one AI combat, animation, reaction, and movement preset. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_AISettingsDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|AI")
	FSAISettings AISettings;
};
