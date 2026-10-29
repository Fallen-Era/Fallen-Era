// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldGenerator/Data/WorldSystemData.h"
#include "WorldGenerator/Data/Payload.h"
#include "VoxelMinimal/VoxelObjectPtr.h"

#include "WorldGeneratorSubsystem.generated.h"



/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldGeneratorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	bool CreateWorld(const FWorldCreateRequest& CreateParams);
	bool BindBiomeOwner(UObject& Owner);
	void UnbindBiomeOwner(UObject& Owner);
	void ClearBiomeBindings();
	
	int32 GetWorldSeed_GameThread() const
	{
		check(IsInGameThread());
		return GenerationSeed;
	}
protected:
	/*
	 * World에 흩 뿌릴 Region Seed들을 생성한다.
	 */
	bool GenerateRegionSites(const FBox2D& WorldBounds, 
							const int32 StreamSeed,
							const int32 RegionCount);
	
	/*
	 * 흩 뿌려진 Region Seed들을 가지고 Region Edges를 Cache한다.
	 */
	bool GenerateVoronoiRegions(FBox2D Bounds, FGeneratedRegionData& Data);
	
	void AssignRandomBiomeTags(FRandomStream& Stream, FGeneratedRegionData& Data);
	
	void OptimizeBiomeAdjacency(const FRegionGenerationSettings& Settings, FGeneratedRegionData& Data);
	
	void ValidateBiomeConnectivity();
	
	EBiomeType GetBiomeAt(FVector2D WorldPosition);
	
	FVector2D WarpOffset(const FVector2D& Position) const;
	int32 FindNearestOriginalSite(const FVector2D& Position) const;
	void ResetWarpState();
	
	FRegionGenerationSettings ActiveGenerationSettings{};
	
	FVector2D WarpPhaseX = FVector2D::ZeroVector;
	FVector2D WarpPhaseY = FVector2D::ZeroVector;
	
	void ClearGenerator();

private:
	UPROPERTY()
	TArray<FRegionSite> RegionSites;
	
	UPROPERTY()
	TArray<FRegionEdge> RegionEdges;
	
	void BuildBiomeSnapshot();
	void ClearBiomeSnapshot();
	
	FBiomeQuerySnapshotPtr BiomeSnapshot;
	TArray<FVoxelObjectPtr> BoundBiomeOwners;
	
	int32 GenerationSeed = 0;
};
