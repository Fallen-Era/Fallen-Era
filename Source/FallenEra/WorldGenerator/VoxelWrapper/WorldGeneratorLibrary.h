// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelFunctionLibrary.h"
#include "Buffer/VoxelBaseBuffers.h"
#include "Buffer/VoxelDoubleBuffers.h"
#include "WorldGeneratorLibrary.generated.h"

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldGeneratorLibrary : public UVoxelFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(Category = "Query")
	FVoxelInt32Buffer QueryBiomeFromPosition(const FVoxelDoubleVector2DBuffer& WorldPosition) const;
};
