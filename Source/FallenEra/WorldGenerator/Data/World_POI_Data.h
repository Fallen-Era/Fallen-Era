// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldSystemData.h"
#include "World_POI_Data.generated.h"

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorld_POI_Data : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FWorldSectorDefinition> SectorDefinitions;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FPOI_Prefab> POI_Prefabs;
	
};
