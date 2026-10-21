#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FEMonsterSpawnDeveloperSettings.generated.h"

class UFE_MonsterSpawnProfileDataAsset;

/** Project-wide defaults for player-centered open-world monster population. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="FallenEra Monster Spawn"))
class FALLENERA_API UFE_MonsterSpawnDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="FallenEra|Spawn")
	TSoftObjectPtr<UFE_MonsterSpawnProfileDataAsset> DefaultOpenWorldSpawnProfile;

	/** XY spatial-hash size used to merge overlapping player spawn areas. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="FallenEra|Spawn", meta=(ClampMin="500.0"))
	float SpawnCellSize = 5000.0f;

	/** Includes ambient and encounter enemies so different systems cannot stack unlimited actors in one cell. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="FallenEra|Spawn", meta=(ClampMin="1"))
	int32 MaxAliveEnemiesPerCell = 4;

	/** Excludes bases/floors even when they generate valid navigation. Zero disables the check. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="FallenEra|Spawn", meta=(ClampMin="0.0", Units="cm"))
	float MinDistanceFromStructures = 1000.0f;
};
