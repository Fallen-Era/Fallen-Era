// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldRegistrySubsystem.h"

#include "EngineUtils.h"
#include "FallenEra.h"
#include "GameplayTag/WorldGameplayTag.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Graphs/VoxelHeightGraph.h"
#include "HAL/IConsoleManager.h"

#include "Heightmap/VoxelHeightmap.h"
#include "VoxelStampActor.h"
#include "VoxelStampManager.h"
#include "VoxelWorld.h"
#include "VoxelDependency.h"
#include "VoxelLayerStack.h"

#include "Graphs/VoxelOutputNode_OutputHeight.h"
#include "VoxelGraphContext.h"
#include "VoxelGraphEnvironment.h"
#include "VoxelGraphPositionParameter.h"
#include "VoxelNodeEvaluator.h"
#include "WorldPersistenceTracker.h"

#include "Graphs/VoxelHeightGraph.h"
#include "Graphs/VoxelHeightGraphStampRef.h"

#include "Settings/WorldGeneratorSettings.h"
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
	
	WorldGeneratorSettings = GetDefault<UWorldGeneratorSettings>();
	
	UGameplayMessageSubsystem* MessageSubsystem =
		Collection.InitializeDependency<UGameplayMessageSubsystem>();
	
	WorldCreateRequestHandle =
		MessageSubsystem->RegisterListener<FWorldCreateRequest>(
		WorldGameplayTag::TAG_Event_World_CreateRequested,
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
	const FIntPoint WorldSize = Request.WorldSize;
	
	UWorld* World = GetWorld();
	
	if (!IsValid(World) || !IsValid(WorldGeneratorSettings))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : World or generator settings is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	
	
	if (WorldSize.X <= 0 || WorldSize.Y <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : World size must be positive."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	UVoxelHeightGraph* HeightGraph = WorldGeneratorSettings->VoxelGraph.LoadSynchronous();
	
	if (!HeightGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Voxel Height Graph invalid, Please Check World Generator Settings. "), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(
		UGameplayStatics::GetActorOfClass(World,AVoxelWorld::StaticClass()));
	
	if (!IsValid(VoxelWorld) || !IsValid(VoxelWorld->LayerStack))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Voxel World or Layer Stack is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	const auto& HeightLayers = VoxelWorld->LayerStack->HeightLayers;
	
	if (HeightLayers.IsEmpty() || !IsValid(HeightLayers[0].Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : A valid Height Layer is required."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	// 1. 생성에 사용할 Stamp 데이터를 생성
	FVoxelHeightGraphStampRef Stamp = FVoxelHeightGraphStampRef::New();
	
	Stamp->Graph = HeightGraph;
	Stamp->Layer = HeightLayers[0].Get();
	Stamp->Transform = FTransform::Identity;
	
	// 2. 유저 입력을 Stamp의 Graph 파라미터로 적용.
	FString Error;
	
	
	// World Size 지정
	FBox2D Bounds = FBox2D(-WorldSize, WorldSize);
	
	if (!Stamp->SetParameter(TEXT("Bounds"), Bounds, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Failed to configure terrain parameters %s"), *GetClass()->GetName(), TEXT(__FUNCTION__), *Error);
		return;
	}
	
	// 3. Level에 Stamp Actor를 생성.
	FActorSpawnParameters Param;
	Param.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;;
	
	AVoxelStampActor* StampActor = 
		World->SpawnActor<AVoxelStampActor>(AVoxelStampActor::StaticClass(), FTransform::Identity, Param);
	
	if (!IsValid(StampActor))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Failed to spawn terrain Stamp Actor."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	// 4. Stamp 설정 적용
	// SetStamp 내부에서 복사 및 등록/갱신
	StampActor->SetStamp(Stamp);
	
	// 5. 자동 실행 부분인데 생성 단계에서는 사용 안할거임.
	// if (!VoxelWorld->IsRuntimeCreated())
	// {
	// 	VoxelWorld->CreateRuntime();
	// }

	UE_LOG(LogTemp, Log, TEXT("%s::%s : Terrain generation requested."), *GetClass()->GetName(), TEXT(__FUNCTION__));
	
	
	SaveWorld();
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
	
	FGuid WorldId = ActiveTracker->GetWorldId();
	
	UWorldProfileSaveGame* Save = NewObject<UWorldProfileSaveGame>();
	
	Save->Profile.WorldId = WorldId;
	
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

void UWorldRegistrySubsystem::LoadRegistry()
{
	
}

void UWorldRegistrySubsystem::SaveRegistry()
{
}


