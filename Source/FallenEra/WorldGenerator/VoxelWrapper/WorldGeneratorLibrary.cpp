// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldGeneratorLibrary.h"

#include "WorldGenerator/Adapters/BiomeQueryBridge.h"
#include "VoxelGraphContext.h"
#include "VoxelGraphEnvironment.h"

VOXEL_REGISTER_FUNCTION(UWorldGeneratorLibrary, QueryBiomeFromPosition);

FVoxelInt32Buffer UWorldGeneratorLibrary::QueryBiomeFromPosition(const FVoxelDoubleVector2DBuffer& WorldPosition) const
{
	const FBiomeQuerySnapshotPtr Snapshot = UBiomeQueryBridge::Find(Query->Context.Environment.Owner);
	
	FVoxelInt32Buffer Result;
	Result.Allocate(WorldPosition.Num());
	
	if (!Snapshot)
	{
		VOXEL_MESSAGE(Error, "{0}: No biome snapshot bound to this graph owner", this);
	}
	
	for (int32 Index =0; Index < WorldPosition.Num(); Index++)
	{
		const EBiomeType Biome = Snapshot 
			? Snapshot->GetBiomeAt(WorldPosition[Index])
			: EBiomeType::TemperateForest;
		
		Result.Set(Index, static_cast<int32>(Biome));
	}
	
	return Result;
}
