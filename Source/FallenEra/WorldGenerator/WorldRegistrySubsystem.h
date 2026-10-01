// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Payload.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Data/Payload.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Save/WorldRegistrySaveGame.h"
#include "WorldRegistrySubsystem.generated.h"

class UWorldSaveGame;
class UWorldRegistrySaveGame;
class UWorldGeneratorSettings;
class UVoxelHeightLayer;
class UVoxelHeightGraph;
class UWorldPersistenceTracker;

struct FVoxelHeightGraphStampRef;
/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	virtual void Deinitialize() override;
	
public:
	
	UWorldRegistrySaveGame* GetRegistry() { return WorldRegistry; } 
	
	bool GetWorldProfile(
		const FGuid& WorldId,
		FWorldDefinitionData& OutProfile) const;
	
	bool SaveWorld();
	void LoadWorld(const FString SlotName);
	
	void RemoveRegistry(FWorldRegistryData& ExistEntry);
	
	TArray<FWorldRegistryData>& GetWorldRegistryList() const { return WorldRegistry->GetWorldRegistryList(); }
	
private:
	void OpenInitWorld();
	
	void CreateWorld(
		FGameplayTag Channel,
		const FWorldCreateRequest& Request);
	
	
	
	
	void DeleteWorld(
	const FGuid& WorldId);
	
	
	void AddRegistry(FWorldRegistryData& NewEntry);
	
	
	
	
	bool MakeTerrainStamp(
		UVoxelHeightGraph* Graph,
		UVoxelHeightLayer* Layer,
		int32 Seed, float Amplitude,
		FVoxelHeightGraphStampRef& OutStamp,
		FString& OutError);
	
private:
	void LoadRegistry();
	void SaveRegistry();
	
	UPROPERTY()
	const UWorldGeneratorSettings* WorldGeneratorSettings = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UWorldPersistenceTracker> ActiveTracker;
	
	UPROPERTY()
	TObjectPtr<UWorldRegistrySaveGame> WorldRegistry = nullptr;
	
	FGameplayMessageListenerHandle WorldCreateRequestHandle;
	
	static const FString RegistrySlotName;
	static constexpr int32 RegistryUserIndex = 0;
	
	
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void ClearPendingLoad();
	
	UPROPERTY(Transient)
	TObjectPtr<UWorldSaveGame> PendingWorldSave;
	
	FString PendingSlotName;
	FString PendingMapPackage;
	
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle TravelFailureHandle;
};
