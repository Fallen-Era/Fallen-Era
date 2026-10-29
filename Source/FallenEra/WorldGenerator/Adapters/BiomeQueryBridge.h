// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VoxelMinimal/VoxelObjectPtr.h"
#include "WorldGenerator/Data/WorldSystemData.h"
#include "BiomeQueryBridge.generated.h"

/**
 * 
 */
UCLASS()
class FALLENERA_API UBiomeQueryBridge : public UObject
{
	GENERATED_BODY()
	
public:
	static void Bind(const FVoxelObjectPtr& Owner, FBiomeQuerySnapshotPtr Snapshot);
	
	static FBiomeQuerySnapshotPtr Find(const FVoxelObjectPtr& Owner);
	
	static void Unbind(const FVoxelObjectPtr& Owner);
};
