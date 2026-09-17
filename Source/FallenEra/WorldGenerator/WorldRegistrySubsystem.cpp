// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldRegistrySubsystem.h"

#include "EngineUtils.h"
#include "FallenEra.h"
#include "FallenEraGameplayTags.h"
#include "VoxelLayerStack.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Graphs/VoxelHeightGraph.h"
#include "HAL/IConsoleManager.h"
#include "Heightmap/VoxelHeightmap.h"
#include "VoxelStampActor.h"
#include "VoxelStampManager.h"
#include "Graphs/VoxelHeightGraphStampRef.h"
#include "Heightmap/VoxelHeightmap_Height.h"
#include "VoxelWorld.h"
#include "../../../../../Program Files/Epic Games/UE_5.8/Engine/Plugins/Media/BlackmagicMedia/Source/ThirdParty/Build/Include/DeckLinkAPI_h.h"
#include "Heightmap/VoxelHeightmapStamp.h"
#include "Kismet/GameplayStatics.h"
#include "Save/WorldProfileSaveGame.h"

#if !UE_BUILD_SHIPPING
namespace
{
void LogTerrainAmplitude(UWorld* World)
{
	// Temporary, read-only diagnostic for the WorldGenerator test asset.
	const TCHAR* GraphPath = TEXT("/Game/WorldGenerator/NewVoxelHeightGraph.NewVoxelHeightGraph");
	UVoxelHeightGraph* Graph = LoadObject<UVoxelHeightGraph>(nullptr, GraphPath);
	if (!Graph)
	{
		UE_LOG(LogFallenEra, Warning, TEXT("[VoxelAmplitude] Could not load %s"), GraphPath);
		return;
	}

	const auto LogAmplitude = [](const IVoxelParameterOverridesOwner& Owner, const FString& Source)
	{
		FString Error;
		// Keep the exposed type intact: Amplitude may be Float or Double.
		const FVoxelPinValue Value = Owner.GetParameter(TEXT("Amplitude"), &Error);
		if (!Value.IsValid())
		{
			UE_LOG(LogFallenEra, Warning, TEXT("[VoxelAmplitude] %s: %s"), *Source, *Error);
			return;
		}

		UE_LOG(LogFallenEra, Log, TEXT("[VoxelAmplitude] %s: Amplitude=%s, Type=%s"),
			*Source, *Value.ExportToString(), *Value.GetType().ToString());
	};

	LogAmplitude(*Graph, FString::Printf(TEXT("Graph asset %s"), GraphPath));
	if (!World)
	{
		UE_LOG(LogFallenEra, Warning, TEXT("[VoxelAmplitude] No World: only the graph asset value was read."));
		return;
	}

	int32 NumMatchingStamps = 0;
	for (TActorIterator<AVoxelStampActor> It(World); It; ++It)
	{
		const FVoxelHeightGraphStampRef Stamp = It->GetStamp().CastTo<FVoxelHeightGraphStamp>();
		if (!Stamp.IsValid() || Stamp->Graph.Get() != Graph)
		{
			continue;
		}

		++NumMatchingStamps;
		// Reads this placed stamp's enabled override, falling back to its graph.
		LogAmplitude(*Stamp, FString::Printf(TEXT("Placed stamp %s [%s]"),
			*It->GetActorNameOrLabel(), *It->GetPathName()));
	}

	if (NumMatchingStamps == 0)
	{
		UE_LOG(LogFallenEra, Warning,
			TEXT("[VoxelAmplitude] No loaded stamp uses this graph in %s. Open Lvl_PCG_World and run WorldGenerator.LogAmplitude there."),
			*World->GetPathName());
	}
}

FAutoConsoleCommandWithWorld LogTerrainAmplitudeCommand(
	TEXT("WorldGenerator.LogAmplitude"),
	TEXT("Log NewVoxelHeightGraph's Amplitude and the effective values of its loaded stamps in the current world."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&LogTerrainAmplitude));
}
#endif


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
#if !UE_BUILD_SHIPPING
	LogTerrainAmplitude(GetWorld());
#endif

	// UVoxelHeightmap* NewHeightMap = new UVoxelHeightmap();
	
	const FIntPoint WorldSize = Request.WorldSize;
	
	// NewHeightMap->Height->SetHeights(WorldSize.X, WorldSize.Y);
}


void UWorldRegistrySubsystem::SaveWorld()
{
	UWorld* CurrentWorld = GetWorld();
	
	if (!IsValid(CurrentWorld))
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s::%s : Current World is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(UGameplayStatics::GetActorOfClass(CurrentWorld, AVoxelWorld::StaticClass()));
	
	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s::%s : Voxel World is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	UVoxelLayerStack* Stack = VoxelWorld->LayerStack;
	if (!Stack)
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s::%s : Stack"), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	const auto Manager = FVoxelStampManager::Get(CurrentWorld);
	Manager->FlushUpdates();
	
	TArray<FVoxelStampRef> UniqueStamps;
	
	const auto ReadLayer = [&](UVoxelLayer* Layer)
	{
		if (!Layer)
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Layer is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
			return;
		}
		
		const auto LayerManager = Manager->FindOrAddLayer(Layer);
		
		
		
		for (const auto& Runtime : LayerManager->GetStamps())
		{
			const FVoxelStampRef StampRef = 
				Runtime->GetWeakStampRef().Pin();
			
			if (StampRef.IsValid())
			{
				UniqueStamps.AddUnique(StampRef);
			}
		}
	};
	
	for (UVoxelHeightLayer* Layer : Stack->HeightLayers)
	{
		ReadLayer(Layer);
	}
	
	for (UVoxelVolumeLayer* Layer : Stack->VolumeLayers)
	{
		ReadLayer(Layer);
	}
	
	UWorldProfileSaveGame* Save = NewObject<UWorldProfileSaveGame>();
	Save->LayerStack = Stack;
	
	for (const FVoxelStampRef& StampRef : UniqueStamps)
	{
		if (!StampRef.IsValid())
		{
			continue;
		}
		
		if (!StampRef.IsA<FVoxelHeightGraphStamp>() && !StampRef.IsA<FVoxelHeightmapStamp>())
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Unsupported stamp type"), *GetClass()->GetName(), TEXT(__FUNCTION__));
			return;
		}
		
		FWorldStampSnapshot& Snapshot = Save->StampSnapshots.AddDefaulted_GetRef();
		
		Snapshot.WorldTransform = StampRef->Transform;
		Snapshot.Stamp = StampRef.MakeCopy();
		
		Snapshot.Stamp->Transform = FTransform::Identity;
	}
	
	if (Save->StampSnapshots.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s : No stamps to save"), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, TEXT("VoxelStampTest"), 0);
	
}

void UWorldRegistrySubsystem::LoadWorld(const FWorldProfileData& WorldProfile)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	UWorldProfileSaveGame* Save = Cast<UWorldProfileSaveGame>(UGameplayStatics::LoadGameFromSlot(("VoxelStampTest"), 0));
	
	if (!Save)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Failed to laod VoxelStampTest"), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	for (TActorIterator<AVoxelStampActor> It(World); It; ++It)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s : Load Test requires a map with no Stamp Actors"), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = 
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	int32 LoadedCount = 0;
	
	for (const FWorldStampSnapshot& Snapshot : Save->StampSnapshots)
	{
		if (!Snapshot.Stamp.IsValid() || 
			!IsValid(Snapshot.Stamp->GetAsset()))
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Invalid stamp or missing asset"), *GetClass()->GetName(), TEXT(__FUNCTION__));
			continue;
		}
		
		AVoxelStampActor* Actor = World->SpawnActor<AVoxelStampActor>(
			AVoxelStampActor::StaticClass(),
			Snapshot.WorldTransform,
			Params);
		
		if (!Actor)
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Failed to spawn Stamp Actor"), *GetClass()->GetName(), TEXT(__FUNCTION__));
			continue;
		}
		
		Actor->SetStamp(Snapshot.Stamp);
		++LoadedCount;
	}

	UE_LOG(LogTemp, Log, TEXT("%s::%s : Stamp restore: %d / %d"), *GetClass()->GetName(), TEXT(__FUNCTION__), LoadedCount, Save->StampSnapshots.Num());
}


