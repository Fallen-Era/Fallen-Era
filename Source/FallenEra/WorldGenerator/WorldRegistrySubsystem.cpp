// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldRegistrySubsystem.h"

#include "FallenEraGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Heightmap/VoxelHeightmap.h"
#include "Heightmap/VoxelHeightmap_Height.h"

void UWorldRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UGameplayMessageSubsystem* MessageSubsystem =
		Collection.InitializeDependency<UGameplayMessageSubsystem>();
	
	WorldCreateRequestHandle =
		MessageSubsystem->RegisterListener<FWorldCreateRequest>(
		FallenEraGameplayTags::TAG_Event_World_CreateRequested,
		this,
		&ThisClass::CreateWorld);
}

void UWorldRegistrySubsystem::Deinitialize()
{
	WorldCreateRequestHandle.Unregister();
	
	Super::Deinitialize();
}

void UWorldRegistrySubsystem::CreateWorld(FGameplayTag Channel, const FWorldCreateRequest& Request)
{
	UVoxelHeightmap* NewHeightMap = new UVoxelHeightmap();
	
	const FIntPoint WorldSize = Request.WorldSize;
	
	NewHeightMap->Height->SetHeights(WorldSize.X, WorldSize.Y);
}
