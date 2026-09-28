// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WorldGeneratorSettings.generated.h"


class UVoxelHeightGraph;

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig)
class FALLENERA_API UWorldGeneratorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Config, Category= "Voxel|Graph")
	TSoftObjectPtr<UVoxelHeightGraph> VoxelGraph; 
	
	
};
