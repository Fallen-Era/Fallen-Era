// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Payload.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Data/Payload.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "WorldRegistrySubsystem.generated.h"

class UVoxelHeightLayer;
class UVoxelHeightGraph;
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
	
	const TArray<FWorldRegistryEntry>& GetWorlds() const;
	
	bool GetWorldProfile(
		const FGuid& WorldId,
		FWorldProfileData& OutProfile) const;
	
private:
	void CreateWorld(
		FGameplayTag Channel,
		const FWorldCreateRequest& Request);
	
	void SaveWorld();
	void LoadWorld(const FWorldProfileData& WorldProfile);
	
	void DeleteWorld(
	const FGuid& WorldId);
	
	void LoadRegistry();
	void SaveRegistry();
	
	bool MakeTerrainStamp(
		UVoxelHeightGraph* Graph,
		UVoxelHeightLayer* Layer,
		int32 Seed, float Amplitude,
		FVoxelHeightGraphStampRef& OutStamp,
		FString& OutError);
	
private:
	UPROPERTY()
	TArray<FWorldRegistryEntry> Worlds;
	
	FGameplayMessageListenerHandle WorldCreateRequestHandle;
};
