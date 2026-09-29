#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MonsterSpawnRegion.generated.h"

class UBoxComponent;
class UFE_MonsterSpawnProfileDataAsset;

/**
 * Optional designer volume retained for asset compatibility and future profile overrides.
 * Ambient population no longer requires or automatically registers this actor.
 */
UCLASS(BlueprintType)
class FALLENERA_API AFE_MonsterSpawnRegion : public AActor
{
	GENERATED_BODY()

public:
	AFE_MonsterSpawnRegion();

	UFUNCTION(BlueprintPure, Category="FallenEra|Spawn")
	bool ContainsLocation(const FVector& WorldLocation) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Spawn")
	TObjectPtr<UBoxComponent> SpawnBounds;

	/** Reserved for a future location-specific override; ignored by player-centered spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|Spawn")
	TSoftObjectPtr<UFE_MonsterSpawnProfileDataAsset> SpawnProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|Spawn")
	bool bSpawnEnabled = true;
};
