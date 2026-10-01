#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Spawn/FEMonsterSpawnTypes.h"
#include "FEMonsterEncounterDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FALLENERA_API FFEMonsterEncounterWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wave")
	TArray<FFEMonsterSpawnEntry> MonsterEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wave", meta=(ClampMin="0"))
	int32 TotalSpawnCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wave", meta=(ClampMin="1"))
	int32 MaxAliveCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wave", meta=(ClampMin="0.0"))
	float InitialDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wave", meta=(ClampMin="0.0"))
	float SpawnInterval = 0.5f;
};

/** Ordered waves spawned around a requested center or matching spawn anchors. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_MonsterEncounterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Encounter")
	TArray<FFEMonsterEncounterWave> Waves;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Encounter|Location", meta=(ClampMin="0.0"))
	float FallbackSpawnRadius = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Encounter|Location", meta=(ClampMin="0.0"))
	float AnchorSearchRadius = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Encounter|Performance")
	FFEMonsterRuntimeDistanceSettings RuntimeDistances;
};
