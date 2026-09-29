#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MonsterSpawnAnchor.generated.h"

/** Optional designer-authored location used by event encounters. */
UCLASS(BlueprintType)
class FALLENERA_API AFE_MonsterSpawnAnchor : public AActor
{
	GENERATED_BODY()

public:
	AFE_MonsterSpawnAnchor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	FName GetSpawnGroup() const { return SpawnGroup; }
	float GetSpawnRadius() const { return SpawnRadius; }
	bool IsSpawnEnabled() const { return bSpawnEnabled; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|Encounter")
	FName SpawnGroup = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|Encounter", meta=(ClampMin="0.0"))
	float SpawnRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|Encounter")
	bool bSpawnEnabled = true;
};
