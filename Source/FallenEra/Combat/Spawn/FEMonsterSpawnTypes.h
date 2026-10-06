#pragma once

#include "CoreMinimal.h"
#include "FEMonsterSpawnTypes.generated.h"

class AFE_EnemyCharacter;

/** Weighted enemy class entry shared by ambient regions and encounter waves. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFEMonsterSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn")
	TSoftClassPtr<AFE_EnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0.0"))
	float Weight = 1.0f;
};

/** Distance thresholds use hysteresis so monsters do not repeatedly activate and despawn at one boundary. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFEMonsterRuntimeDistanceSettings
{
	GENERATED_BODY()

	/** Full server-side AI simulation runs while any player is within this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distance", meta=(ClampMin="0.0"))
	float ActivationDistance = 12000.0f;

	/** Per-client skeletal mesh draw distance. Keep this below ActivationDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distance", meta=(ClampMin="0.0"))
	float VisualCullDistance = 10000.0f;

	/** Per-connection replication distance for spawned enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distance", meta=(ClampMin="0.0"))
	float NetCullDistance = 14000.0f;

	/** Ambient monsters farther than this from every player become despawn candidates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distance", meta=(ClampMin="0.0"))
	float DespawnDistance = 16000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distance", meta=(ClampMin="0.0"))
	float DespawnGracePeriod = 15.0f;

	/** Dead managed enemies are removed after their death presentation has finished. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Distance", meta=(ClampMin="0.0"))
	float CorpseLifetime = 10.0f;
};
