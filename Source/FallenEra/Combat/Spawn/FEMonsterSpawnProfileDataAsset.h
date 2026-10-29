#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Spawn/FEMonsterSpawnTypes.h"
#include "FEMonsterSpawnProfileDataAsset.generated.h"

/** Reusable ambient population rules applied to the merged player-centered cell set. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_MonsterSpawnProfileDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn")
	TArray<FFEMonsterSpawnEntry> MonsterEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Population", meta=(ClampMin="0"))
	int32 DesiredEnemiesPerPlayer = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Population", meta=(ClampMin="0"))
	int32 MaxActiveEnemies = 24;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Population", meta=(ClampMin="1"))
	int32 MaxSpawnsPerUpdate = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Location", meta=(ClampMin="0.0"))
	float MinSpawnDistance = 4000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Location", meta=(ClampMin="0.0"))
	float MaxSpawnDistance = 8000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Location", meta=(ClampMin="1"))
	int32 LocationSearchAttempts = 12;

	/** Reject unobstructed candidates inside a player's forward view cone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Location")
	bool bAvoidVisibleSpawnLocations = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Location",
		meta=(ClampMin="0.0", ClampMax="180.0", EditCondition="bAvoidVisibleSpawnLocations"))
	float VisibleSpawnHalfAngle = 70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Performance", meta=(ClampMin="0.1"))
	float EvaluationInterval = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Spawn|Performance")
	FFEMonsterRuntimeDistanceSettings RuntimeDistances;
};
