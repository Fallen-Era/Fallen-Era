// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldGenerator/Data/WorldSystemData.h"
#include "WorldGenerator/Data/Payload.h"

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
	
	UPROPERTY()
	TArray<int32> WorldWeightGrid;
	
protected:
	
	
	bool CreateWorld(const FWorldCreateRequest& CreateParams);
	
	/*
	 * World에 흩 뿌릴 Region Seed들을 생성한다.
	 */
	bool GenerateRegionSites(const FBox2D& WorldBounds, 
							const int32 StreamSeed,
							const int32 RegionCount);
	
	/*
	 * 흩 뿌려진 Region Seed들을 가지고 Region Edges를 Cache한다.
	 */
	bool GenerateVoronoiRegions();
	
	void AssignRandomBiomeTags();
	
	void OptimizeBiomeAdjacency();
	
	void ValidateBiomeConnectivity();
	
	
	
	void ClearGenerator();
	

	
private:
	UPROPERTY()
	TArray<FRegionSite> RegionSites;
	
	UPROPERTY()
	TArray<FRegionEdge> RegionEdges;
};
