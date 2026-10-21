#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Spawn/FEMonsterSpawnTypes.h"
#include "FEMonsterSpawnSubsystem.generated.h"

class AFE_EnemyCharacter;
class AFE_MonsterSpawnAnchor;
class APawn;
struct FStreamableHandle;
class UFE_MonsterEncounterDataAsset;
class UFE_MonsterSpawnProfileDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFE_OnManagedMonsterSpawned, AFE_EnemyCharacter*, Monster);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFE_OnManagedMonsterDespawned, AFE_EnemyCharacter*, Monster);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFE_OnMonsterEncounterCompleted, int32, EncounterHandle);

USTRUCT()
struct FFEManagedMonsterRuntime
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<AFE_EnemyCharacter> Monster;

	UPROPERTY(Transient)
	FFEMonsterRuntimeDistanceSettings DistanceSettings;

	FIntPoint SpawnCell = FIntPoint::ZeroValue;
	int32 EncounterHandle = INDEX_NONE;
	double DormantStartTime = -1.0;
	double DeathStartTime = -1.0;
	bool bSimulationActive = true;
};

USTRUCT()
struct FFEActiveMonsterEncounter
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UFE_MonsterEncounterDataAsset> Definition;

	FVector CenterLocation = FVector::ZeroVector;
	FName SpawnGroup = NAME_None;
	int32 Handle = INDEX_NONE;
	int32 WaveIndex = 0;
	int32 SpawnedInWave = 0;
	double NextSpawnTime = 0.0;
};

/** Server-authoritative population service for player-centered cells and event encounters. */
UCLASS()
class FALLENERA_API UFE_MonsterSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void RegisterSpawnAnchor(AFE_MonsterSpawnAnchor* SpawnAnchor);
	void UnregisterSpawnAnchor(AFE_MonsterSpawnAnchor* SpawnAnchor);

	/** Starts a server-owned wave encounter. Returns INDEX_NONE when the request is invalid. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Spawn")
	int32 StartEncounter(
		UFE_MonsterEncounterDataAsset* EncounterDefinition,
		FVector CenterLocation,
		FName SpawnGroup = NAME_None);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Spawn")
	void CancelEncounter(int32 EncounterHandle, bool bDestroyRemainingMonsters = true);

	UFUNCTION(BlueprintPure, Category="FallenEra|Spawn")
	int32 GetManagedMonsterCount() const { return ManagedMonsters.Num(); }

	UPROPERTY(BlueprintAssignable, Category="FallenEra|Spawn")
	FFE_OnManagedMonsterSpawned OnMonsterSpawned;

	UPROPERTY(BlueprintAssignable, Category="FallenEra|Spawn")
	FFE_OnManagedMonsterDespawned OnMonsterDespawned;

	UPROPERTY(BlueprintAssignable, Category="FallenEra|Spawn")
	FFE_OnMonsterEncounterCompleted OnEncounterCompleted;

private:
	void UpdateSpawnManagement();
	void LoadDefaultSpawnProfile();
	void HandleDefaultSpawnProfileLoaded();
	void GatherPlayerPawns(TArray<TWeakObjectPtr<APawn>>& OutPlayerPawns) const;
	void UpdateManagedMonsters(const TArray<TWeakObjectPtr<APawn>>& PlayerPawns, double CurrentTime);
	void UpdateAmbientCells(const TArray<TWeakObjectPtr<APawn>>& PlayerPawns, double CurrentTime);
	void UpdateEncounters(const TArray<TWeakObjectPtr<APawn>>& PlayerPawns, double CurrentTime);

	bool TrySpawnAmbientMonster(
		const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
		const TArray<FIntPoint>& AvailableCells);
	bool TrySpawnEncounterMonster(
		FFEActiveMonsterEncounter& Encounter,
		const TArray<TWeakObjectPtr<APawn>>& PlayerPawns);
	AFE_EnemyCharacter* SpawnManagedMonster(
		const FFEMonsterSpawnEntry& SpawnEntry,
		const FTransform& SpawnTransform,
		const FFEMonsterRuntimeDistanceSettings& DistanceSettings,
		int32 EncounterHandle,
		APawn* EncounterTarget = nullptr);

	bool FindAmbientSpawnTransform(
		const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
		const TArray<FIntPoint>& AvailableCells,
		FTransform& OutSpawnTransform) const;
	bool FindEncounterSpawnTransform(
		const FFEActiveMonsterEncounter& Encounter,
		FTransform& OutSpawnTransform) const;
	bool ProjectSpawnCandidate(const FVector& CandidateLocation, FTransform& OutSpawnTransform) const;
	bool IsVisibleToAnyPlayer(const FVector& SpawnLocation, float HalfAngleDegrees) const;
	bool IsSpawnSpaceBlocked(TSubclassOf<AFE_EnemyCharacter> EnemyClass, const FVector& SpawnLocation) const;

	const FFEMonsterSpawnEntry* SelectWeightedEntry(const TArray<FFEMonsterSpawnEntry>& Entries) const;
	void RequestEnemyClassLoad(const TSoftClassPtr<AFE_EnemyCharacter>& EnemyClass);
	void PrepareEnemyAssets(FSoftObjectPath ClassPath, int32 Stage);
	void RebuildCellPopulation();
	TMap<FIntPoint, int32> CellPopulation;
	TSet<FSoftObjectPath> ReadyEnemyClasses;
	TMap<FSoftObjectPath, TArray<TSharedPtr<FStreamableHandle>>> EnemyDependencyLoadHandles;
	float GetNearestPlayerDistanceSquared(
		const FVector& Location,
		const TArray<TWeakObjectPtr<APawn>>& PlayerPawns) const;
	APawn* FindBestEncounterTarget(
		const FVector& Location,
		const TArray<TWeakObjectPtr<APawn>>& PlayerPawns) const;
	void BuildAvailableSpawnCells(
		const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
		TArray<FIntPoint>& OutAvailableCells) const;
	FIntPoint GetSpawnCell(const FVector& WorldLocation) const;
	FVector GetSpawnCellCenter(const FIntPoint& Cell) const;
	int32 CountAliveAmbientMonsters() const;
	int32 CountAliveMonstersInCell(const FIntPoint& Cell) const;
	int32 CountAliveMonstersForEncounter(int32 EncounterHandle) const;
	void RemoveManagedMonsterAt(int32 Index, bool bDestroyMonster);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AFE_MonsterSpawnAnchor>> SpawnAnchors;

	UPROPERTY(Transient)
	TObjectPtr<UFE_MonsterSpawnProfileDataAsset> DefaultSpawnProfile;

	UPROPERTY(Transient)
	TArray<FFEManagedMonsterRuntime> ManagedMonsters;

	UPROPERTY(Transient)
	TArray<FFEActiveMonsterEncounter> ActiveEncounters;

	/** Retaining handles prevents an asynchronously loaded class from unloading between spawn updates. */
	TMap<FSoftObjectPath, TSharedPtr<FStreamableHandle>> EnemyClassLoadHandles;
	TSharedPtr<FStreamableHandle> DefaultProfileLoadHandle;
	FTimerHandle ManagementTimerHandle;
	int32 NextEncounterHandle = 1;
	double NextAmbientEvaluationTime = 0.0;

	static constexpr float ManagementInterval = 0.5f;
};
