// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldGenerator/Data/WorldSystemData.h"
#include "WorldGeneratorSubsystem.generated.h"



/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldGeneratorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	
public:
	UPROPERTY()
	TArray<int32> WorldWeightGrid;
	
protected:
	
	
	/*
	 * World에 흩 뿌릴 Region Seed들을 생성한다.
	 */
	void GenerateRegionSeed();
	
	/*
	 * 흩 뿌려진 Region Seed들을 가지고 Region Edges를 Cache한다.
	 */
	void GenerateVoronoiRegions();

	
private:
	UPROPERTY()
	TArray<FRegionSeed> RegionSeeds;
	
	UPROPERTY()
	TArray<FRegionEdge> RegionEdges;
};
