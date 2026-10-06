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
#include "Async/IAsyncTask.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"

#include "Graphs/VoxelHeightGraph.h"
#include "Graphs/VoxelHeightGraphStampRef.h"

#include "Settings/WorldGeneratorSettings.h"
#include "Heightmap/VoxelHeightmapStamp.h"
#include "Kismet/GameplayStatics.h"
#include "Save/WorldSaveGame.h"
#include "Save/WorldRegistrySaveGame.h"


const FString UWorldRegistrySubsystem::RegistrySlotName = TEXT("WorldRegistry");

void UWorldRegistrySubsystem::HandlePostOpenMap(UWorld* OpenWorld)
{
	if (WorldOpenMode == EWorldOpenMode::None ||
		!IsValid(OpenWorld) ||
		!OpenWorld->IsGameWorld() ||
		OpenWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	
	const FString OpenMapPackage = UWorld::RemovePIEPrefix(
		OpenWorld->GetOutermost()->GetName());
	
	if (OpenMapPackage != PendingMapPackage)
	{
		return;
	}
	
	switch (WorldOpenMode)
	{
	case EWorldOpenMode::Create:
		PostWorldCreate(OpenWorld);
		break;
	case EWorldOpenMode::Load:
		PostWorldLoad(OpenWorld);
		break;
		
	default:
		break;
	}
	
	ClearPending();
}

void UWorldRegistrySubsystem::PostWorldCreate(UWorld* OpenWorld)
{
	const FWorldCreateRequest Request = PendingCreateRequest;
	
	const auto Fail = [this](const TCHAR* Message)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : %s"), *GetClass()->GetName(), TEXT(__FUNCTION__), Message);
	};
	
	if (OpenWorld != GetWorld() ||
		!IsValid(WorldRegistry) ||
		!IsValid(WorldGeneratorSettings))
	{
		Fail(TEXT("World, registry or settings is invalid."));
		return;
	}
	
	AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(UGameplayStatics::GetActorOfClass(OpenWorld, AVoxelWorld::StaticClass()));
	
	if (!IsValid(VoxelWorld) || !IsValid(VoxelWorld->LayerStack))
	{
		Fail(TEXT("Voxel world or LayerStack is invalid."));
		return;
	}
	
	for (TActorIterator<AVoxelStampActor> It(OpenWorld); It; ++It)
	{
		Fail(TEXT("The destniation map must have no Stamp Actors."));
		return;
	}
	
	UVoxelHeightGraph* HeightGraph = WorldGeneratorSettings->VoxelGraph.LoadSynchronous();
	
	if (!IsValid(HeightGraph))
	{
		Fail(TEXT("Voxel Height Graph is invalid."));
		return;
	}
	
	const auto& HeightLayers = VoxelWorld->LayerStack->HeightLayers;
	
	if (HeightLayers.IsEmpty() || !IsValid(HeightLayers[0].Get()))
	{
		Fail(TEXT("A vaild Height Layer is required."));
		return;
	}
	
	FVoxelHeightGraphStampRef Stamp = FVoxelHeightGraphStampRef::New();
	Stamp->Graph = HeightGraph;
	Stamp->Layer = HeightLayers[0].Get();
	Stamp->Transform = FTransform::Identity;
	
	const FVector2D HalfWorldSize(
		Request.WorldSize.X * 0.5f,
		Request.WorldSize.Y * 0.5f);
	
	const FBox2D Bounds(-HalfWorldSize, HalfWorldSize);
	
	FString Error;
	if (!Stamp->SetParameter(TEXT("Bounds"), Bounds, &Error))
	{
		Fail(*FString::Printf(TEXT("Failed to configure terrain parameters: %s"), *Error));
		return;
	}
	
	VoxelWorld->DestroyRuntime();
	
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = 
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AVoxelStampActor* StampActor = OpenWorld->SpawnActor<AVoxelStampActor>(
		AVoxelStampActor::StaticClass(),
		FTransform::Identity,
		Params);
	
	if (!IsValid(StampActor))
	{
		Fail(TEXT("Failed to spawn terrain Stamp Actor."));
		return;
	}
	
	StampActor->SetStamp(Stamp);
	
	FWorldRegistryData Entry;
	Entry.WorldId = FGuid::NewGuid();
	Entry.DisplayName = Request.DisplayName;
	Entry.WorldSize = Request.WorldSize;
	Entry.Diffculty = Request.Diffculty;
	Entry.ProfileSlotName = FString::Printf(
		TEXT("World_%s"),
		*Entry.WorldId.ToString(EGuidFormats::Digits));
	
	ActiveTracker = NewObject<UWorldPersistenceTracker>(this);
	
	ActiveTracker->GetRegistryDataRef() = Entry;
	ActiveTracker->BeginSession();
	
	if (!SaveWorld())
	{
		StampActor->Destroy();
		FVoxelStampManager::Get(OpenWorld)->FlushUpdates();
		ActiveTracker = nullptr;
		
		Fail(TEXT("Failed to save the generated world."));
		return;
	}
	
	AddRegistry(Entry);
	
	VoxelWorld->CreateRuntime();
	
	VoxelWorld->OnNextStateRendered(
		FSimpleDelegate::CreateWeakLambda(this, []
		{
			UE_LOG(LogTemp, Log, TEXT("UWorldRegistrySubsystem::%s : Create voxel terrain rendered."), TEXT(__FUNCTION__));
		}));
}

void UWorldRegistrySubsystem::PostWorldLoad(UWorld* OpenWorld)
{
	if (!PendingWorldSave)
	{
		return;
	}
	
	AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(UGameplayStatics::GetActorOfClass(OpenWorld, AVoxelWorld::StaticClass()));
		
	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : The destination map has no VoxelWorld."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	for (TActorIterator<AVoxelStampActor> It(OpenWorld); It; ++It)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : The destination map must have no Stamp Actors."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	VoxelWorld->DestroyRuntime();
	
	UWorldSaveGame* Save = PendingWorldSave.Get();
	VoxelWorld->LayerStack = Save->LayerStack;
	
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	TArray<AVoxelStampActor*> SpawnActors;
	
	for (const FWorldStampSnapshot& Snapshot : Save->StampSnapshots)
	{
		AVoxelStampActor* Actor = OpenWorld->SpawnActor<AVoxelStampActor>(
			AVoxelStampActor::StaticClass(),
			Snapshot.WorldTransform,
			Params);
		
		if (!IsValid(Actor))
		{
			for (AVoxelStampActor* SpawnActor : SpawnActors)
			{
				SpawnActor->Destroy();
			}
			
			FVoxelStampManager::Get(OpenWorld)->FlushUpdates();

			UE_LOG(LogTemp, Error, TEXT("%s::%s : Failed to restore world stmaps."), *GetClass()->GetName(), TEXT(__FUNCTION__));
			
			return;
		}
		
		Actor->SetStamp(Snapshot.Stamp);
		SpawnActors.Add(Actor);
	}
	
	FVoxelStampManager::Get(OpenWorld)->FlushUpdates();
	
	ActiveTracker = NewObject<UWorldPersistenceTracker>(this);
	ActiveTracker->SetWorldId(Save->Definition.WorldId);
	ActiveTracker->SetSlotName(PendingSlotName);
	ActiveTracker->SetSessionId(FGuid::NewGuid());
	
	ActiveTracker->GetRegistryDataRef().WorldSize = Save->Definition.WorldSize;

	UE_LOG(LogTemp, Log, TEXT("%s::%s : World restored : %s, %d stamps"), *GetClass()->GetName(), TEXT(__FUNCTION__),
		*PendingSlotName,
		SpawnActors.Num());
	
	VoxelWorld->CreateRuntime();
	
	VoxelWorld->OnNextStateRendered(
		FSimpleDelegate::CreateWeakLambda(this, []
		{
			UE_LOG(LogTemp, Log, TEXT("OnNextStateRendered|FSimpleDelegate::CreateWeakLambda : Restored voxel terrain rendered."));
		}));
}


void UWorldRegistrySubsystem::ClearPending()
{
	PendingWorldSave = nullptr;
	PendingCreateRequest = FWorldCreateRequest{};
	PendingSlotName.Reset();
	PendingMapPackage.Reset();
	WorldOpenMode = EWorldOpenMode::None;
}


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
	
	ActiveTracker = NewObject<UWorldPersistenceTracker>();
	
	PostLoadMapHandle = 
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostOpenMap);
	
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddLambda(
			[this](
				UWorld* FailedWorld,
				ETravelFailure::Type FailureType,
				const FString& Error)
			{
				if (WorldOpenMode == EWorldOpenMode::None ||
					!FailedWorld ||
					FailedWorld->GetGameInstance() != GetGameInstance())
				{
					return;
				}

				UE_LOG(LogTemp, Error, TEXT("%s::%s : World travel failed: %s"), *GetClass()->GetName(), TEXT(__FUNCTION__), *Error);
				
				ClearPending();
			});
	}
	
	LoadRegistry();
}

void UWorldRegistrySubsystem::Deinitialize()
{
	WorldCreateRequestHandle.Unregister();
	
	SaveRegistry();
	
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	
	ClearPending();
	
	Super::Deinitialize();
}



void UWorldRegistrySubsystem::CreateWorld(FGameplayTag Channel, const FWorldCreateRequest& Request)
{
	if (WorldOpenMode != EWorldOpenMode::None)
	{
		return;
	}
	
	if (!WorldRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : World registry is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}

	if (WorldRegistry->Find(Request.DisplayName) != INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s : A world with the same name already exists."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		// 오류 반환
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
		
		FMessageBoxRequest MessageBoxRequest = 
		{
			FText::FromString(TEXT("Warrning")),
			FText::FromString(TEXT("A World with the same name already exists.")),
			EMessageBoxType::Yes
		};
		
		MessageSubsystem.BroadcastMessage(WorldGameplayTag::TAG_Event_World_Widget_MessageBox, MessageBoxRequest);
		
		return;
	}
	
	const FIntPoint WorldSize = Request.WorldSize;
	
	if (WorldSize.X <= 0 || WorldSize.Y <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : World size must be positive."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	
	PendingCreateRequest = Request;
	WorldOpenMode = EWorldOpenMode::Create;
	
	OpenInitWorld();
	
}


bool UWorldRegistrySubsystem::SaveWorld()
{
	UWorld* CurrentWorld = GetWorld();
	
	if (!IsValid(CurrentWorld))
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s::%s : Current World is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
	}
	
	AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(UGameplayStatics::GetActorOfClass(CurrentWorld, AVoxelWorld::StaticClass()));
	
	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s::%s : Voxel World is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
	}
	
	UVoxelLayerStack* Stack = VoxelWorld->LayerStack;
	if (!Stack)
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s::%s : Stack"), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
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
	
	if (!IsValid(ActiveTracker))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : ActiveTracker is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
	}
	
	FWorldRegistryData& MetaData = ActiveTracker->GetRegistryDataRef();
	
	UWorldSaveGame* Save = NewObject<UWorldSaveGame>();
	
	
	
	Save->Definition.WorldId = MetaData.WorldId;
	Save->Definition.WorldSize = MetaData.WorldSize;
	
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
			return false;
		}
		
		FWorldStampSnapshot& Snapshot = Save->StampSnapshots.AddDefaulted_GetRef();
		
		Snapshot.WorldTransform = StampRef->Transform;
		Snapshot.Stamp = StampRef.MakeCopy();
		
		Snapshot.Stamp->Transform = FTransform::Identity;
	}
	
	if (Save->StampSnapshots.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s : No stamps to save"), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
	}
	
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, ActiveTracker->GetSlotName(), 0);
	return bSaved;
	
}

void UWorldRegistrySubsystem::OpenInitWorld()
{
	const UWorldGeneratorSettings* Settings = GetDefault<UWorldGeneratorSettings>();
	
	if (!IsValid(GetWorld()) ||
		!Settings ||
		Settings->InitVoxelWorld.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : World or InitVoxelWorld is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		
		ClearPending();
		return;
	}
	
	PendingMapPackage = Settings->InitVoxelWorld.ToSoftObjectPath().GetLongPackageName();
	
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Settings->InitVoxelWorld);
}

void UWorldRegistrySubsystem::LoadWorld(const FString SlotName)
{
	if (WorldOpenMode != EWorldOpenMode::None)
	{
		return;
	}
	
	if (!GetWorld() || SlotName.IsEmpty())
	{
		return;
	}
	
	UWorldSaveGame* Save = Cast<UWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	
	if (!Save ||
		!Save->Definition.WorldId.IsValid() ||
		!IsValid(Save->LayerStack.Get()) ||
		Save->StampSnapshots.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Invalid world save : %s"), *GetClass()->GetName(), TEXT(__FUNCTION__), *SlotName);
		return;
	}
	
	for (const FWorldStampSnapshot& Snapshot : Save->StampSnapshots)
	{
		if (!Snapshot.Stamp.IsValid() ||
			!IsValid(Snapshot.Stamp->GetAsset()))
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Invalid stamp or missing asset in %s."), *GetClass()->GetName(), TEXT(__FUNCTION__), *SlotName);
			return;
		}
	}
	
	PendingWorldSave = Save;
	PendingSlotName = SlotName;
	WorldOpenMode = EWorldOpenMode::Load;
	
	OpenInitWorld();
	
}

void UWorldRegistrySubsystem::AddRegistry(FWorldRegistryData& NewEntry)
{
	WorldRegistry->AddRegistry(NewEntry);
	SaveRegistry();
}

void UWorldRegistrySubsystem::RemoveRegistry(FWorldRegistryData& ExistEntry)
{
	
	if (!WorldRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : World registry is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}

	const int32 Index = WorldRegistry->Find(ExistEntry.WorldId);
	if (Index != INDEX_NONE)
	{
		const FWorldRegistryData RemovedEntry = WorldRegistry->GetWorldRegistryList()[Index];
		WorldRegistry->RemoveRegistry(Index);
		UE_LOG(LogTemp, Log, TEXT("%s::%s : Removed registry '%s' at index %d (WorldId=%s)."),
			*GetClass()->GetName(), TEXT(__FUNCTION__), *RemovedEntry.DisplayName, Index, *RemovedEntry.WorldId.ToString());
		SaveRegistry();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%s : Registry entry not found: '%s' (WorldId=%s)."),
			*GetClass()->GetName(), TEXT(__FUNCTION__), *ExistEntry.DisplayName, *ExistEntry.WorldId.ToString());
		return;
	}
}



void UWorldRegistrySubsystem::LoadRegistry()
{
	if (UGameplayStatics::DoesSaveGameExist(
		RegistrySlotName,
		RegistryUserIndex))
	{
		USaveGame* LoadedSaveGame = 
			UGameplayStatics::LoadGameFromSlot(RegistrySlotName, RegistryUserIndex);
		
		WorldRegistry = Cast<UWorldRegistrySaveGame>(LoadedSaveGame);
	}
	
	if (!WorldRegistry)
	{
		WorldRegistry = Cast<UWorldRegistrySaveGame>(UGameplayStatics::CreateSaveGameObject(UWorldRegistrySaveGame::StaticClass()));
		
		if (!WorldRegistry)
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Failed to create WorldRegistrySaveGame."), *GetClass()->GetName(), TEXT(__FUNCTION__));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("%s::%s : Create New World Registry."), *GetClass()->GetName(), TEXT(__FUNCTION__));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("%s::%s : Loaded World Registry (%d entries)."),
			*GetClass()->GetName(), TEXT(__FUNCTION__), WorldRegistry->GetWorldRegistryList().Num());
	}

	for (const FWorldRegistryData& Entry : WorldRegistry->GetWorldRegistryList())
	{
		if (Entry.WorldSize.X <= 0 || Entry.WorldSize.Y <= 0 ||
			static_cast<uint8>(Entry.Diffculty) > static_cast<uint8>(EWorldDiffculty::InTheHell))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("%s::%s : Invalid saved metadata for '%s': WorldId=%s, Size=%d x %d, Difficulty=%d, Slot='%s'."),
				*GetClass()->GetName(), TEXT(__FUNCTION__), *Entry.DisplayName, *Entry.WorldId.ToString(),
				Entry.WorldSize.X, Entry.WorldSize.Y, static_cast<uint8>(Entry.Diffculty), *Entry.ProfileSlotName);
		}
	}
	
}

void UWorldRegistrySubsystem::SaveRegistry()
{
	if (WorldRegistry)
	{
		if (!UGameplayStatics::SaveGameToSlot(WorldRegistry, RegistrySlotName, RegistryUserIndex))
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%s : Failed to save registry slot '%s'."),
				*GetClass()->GetName(), TEXT(__FUNCTION__), *RegistrySlotName);
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("%s::%s : Saved Registry."), *GetClass()->GetName(), TEXT(__FUNCTION__));
	}
	else
	{
		LoadRegistry();
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Registry save failed. Reason: Runtime object is invalid. Attempting to reload. Please try saving again."), *GetClass()->GetName(), TEXT(__FUNCTION__));
	}
}


