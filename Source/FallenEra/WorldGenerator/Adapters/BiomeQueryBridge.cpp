// Fill out your copyright notice in the Description page of Project Settings.


#include "BiomeQueryBridge.h"


namespace
{
	FRWLock BiomeBridgeLock;
	
	TMap<FVoxelObjectPtr, FBiomeQuerySnapshotPtr> BiomeSnapshots;
}

void UBiomeQueryBridge::Bind(const FVoxelObjectPtr& Owner, FBiomeQuerySnapshotPtr Snapshot)
{
	check(IsInGameThread());
	
	FWriteScopeLock Scope(BiomeBridgeLock);
	BiomeSnapshots.Add(Owner, MoveTemp(Snapshot));
}

FBiomeQuerySnapshotPtr UBiomeQueryBridge::Find(const FVoxelObjectPtr& Owner)
{
	FReadScopeLock Scope(BiomeBridgeLock);
	return BiomeSnapshots.FindRef(Owner);
}

void UBiomeQueryBridge::Unbind(const FVoxelObjectPtr& Owner)
{
	check(IsInGameThread());
	
	FWriteScopeLock Scope(BiomeBridgeLock);
	BiomeSnapshots.Remove(Owner);
}
