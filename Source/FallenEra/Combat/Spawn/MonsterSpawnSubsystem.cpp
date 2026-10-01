#include "Combat/Spawn/MonsterSpawnSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Combat/AI/AITargetCoordinatorSubsystem.h"
#include "Combat/AI/EnemyAIController.h"
#include "Combat/Character/EnemyCharacter.h"
#include "Combat/Spawn/MonsterEncounterDataAsset.h"
#include "Combat/Spawn/MonsterSpawnAnchor.h"
#include "Combat/Spawn/MonsterSpawnDeveloperSettings.h"
#include "Combat/Spawn/MonsterSpawnProfileDataAsset.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

void UFE_MonsterSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	LoadDefaultSpawnProfile();

	InWorld.GetTimerManager().SetTimer(
		ManagementTimerHandle,
		this,
		&UFE_MonsterSpawnSubsystem::UpdateSpawnManagement,
		ManagementInterval,
		true,
		FMath::FRandRange(0.0f, ManagementInterval));
}

void UFE_MonsterSpawnSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ManagementTimerHandle);
	}
	for (TPair<FSoftObjectPath, TSharedPtr<FStreamableHandle>>& Pair : EnemyClassLoadHandles)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->CancelHandle();
		}
	}
	EnemyClassLoadHandles.Reset();
	if (DefaultProfileLoadHandle.IsValid())
	{
		DefaultProfileLoadHandle->CancelHandle();
		DefaultProfileLoadHandle.Reset();
	}
	DefaultSpawnProfile = nullptr;
	SpawnAnchors.Reset();
	ManagedMonsters.Reset();
	ActiveEncounters.Reset();
	Super::Deinitialize();
}

void UFE_MonsterSpawnSubsystem::LoadDefaultSpawnProfile()
{
	const UFE_MonsterSpawnDeveloperSettings* Settings = GetDefault<UFE_MonsterSpawnDeveloperSettings>();
	if (!Settings || Settings->DefaultOpenWorldSpawnProfile.IsNull())
	{
		return;
	}

	if (UFE_MonsterSpawnProfileDataAsset* LoadedProfile =
		Settings->DefaultOpenWorldSpawnProfile.Get())
	{
		DefaultSpawnProfile = LoadedProfile;
		for (const FFEMonsterSpawnEntry& Entry : DefaultSpawnProfile->MonsterEntries)
		{
			RequestEnemyClassLoad(Entry.EnemyClass);
		}
		return;
	}
	DefaultProfileLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Settings->DefaultOpenWorldSpawnProfile.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(
			this, &UFE_MonsterSpawnSubsystem::HandleDefaultSpawnProfileLoaded));
}

void UFE_MonsterSpawnSubsystem::HandleDefaultSpawnProfileLoaded()
{
	const UFE_MonsterSpawnDeveloperSettings* Settings = GetDefault<UFE_MonsterSpawnDeveloperSettings>();
	DefaultSpawnProfile = Settings ? Settings->DefaultOpenWorldSpawnProfile.Get() : nullptr;
	if (DefaultSpawnProfile)
	{
		for (const FFEMonsterSpawnEntry& Entry : DefaultSpawnProfile->MonsterEntries)
		{
			RequestEnemyClassLoad(Entry.EnemyClass);
		}
	}
}

void UFE_MonsterSpawnSubsystem::RegisterSpawnAnchor(AFE_MonsterSpawnAnchor* SpawnAnchor)
{
	if (IsValid(SpawnAnchor) && SpawnAnchor->HasAuthority())
	{
		SpawnAnchors.AddUnique(SpawnAnchor);
	}
}

void UFE_MonsterSpawnSubsystem::UnregisterSpawnAnchor(AFE_MonsterSpawnAnchor* SpawnAnchor)
{
	SpawnAnchors.Remove(SpawnAnchor);
}

int32 UFE_MonsterSpawnSubsystem::StartEncounter(
	UFE_MonsterEncounterDataAsset* EncounterDefinition,
	FVector CenterLocation,
	FName SpawnGroup)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !EncounterDefinition || EncounterDefinition->Waves.IsEmpty())
	{
		return INDEX_NONE;
	}

	FFEActiveMonsterEncounter& Encounter = ActiveEncounters.AddDefaulted_GetRef();
	Encounter.Definition = EncounterDefinition;
	Encounter.CenterLocation = CenterLocation;
	Encounter.SpawnGroup = SpawnGroup;
	Encounter.Handle = NextEncounterHandle++;
	Encounter.NextSpawnTime = World->GetTimeSeconds() +
		FMath::Max(0.0f, EncounterDefinition->Waves[0].InitialDelay);

	for (const FFEMonsterEncounterWave& Wave : EncounterDefinition->Waves)
	{
		for (const FFEMonsterSpawnEntry& Entry : Wave.MonsterEntries)
		{
			RequestEnemyClassLoad(Entry.EnemyClass);
		}
	}
	return Encounter.Handle;
}

void UFE_MonsterSpawnSubsystem::CancelEncounter(
	int32 EncounterHandle,
	bool bDestroyRemainingMonsters)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	for (int32 Index = ManagedMonsters.Num() - 1; Index >= 0; --Index)
	{
		if (ManagedMonsters[Index].EncounterHandle == EncounterHandle)
		{
			if (bDestroyRemainingMonsters)
			{
				RemoveManagedMonsterAt(Index, true);
			}
			else
			{
				ManagedMonsters[Index].EncounterHandle = INDEX_NONE;
			}
		}
	}
	ActiveEncounters.RemoveAll(
		[EncounterHandle](const FFEActiveMonsterEncounter& Encounter)
		{
			return Encounter.Handle == EncounterHandle;
		});
}

void UFE_MonsterSpawnSubsystem::UpdateSpawnManagement()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	TArray<TWeakObjectPtr<APawn>> PlayerPawns;
	GatherPlayerPawns(PlayerPawns);
	const double CurrentTime = World->GetTimeSeconds();
	UpdateManagedMonsters(PlayerPawns, CurrentTime);
	UpdateEncounters(PlayerPawns, CurrentTime);
	UpdateAmbientCells(PlayerPawns, CurrentTime);
}

void UFE_MonsterSpawnSubsystem::GatherPlayerPawns(
	TArray<TWeakObjectPtr<APawn>>& OutPlayerPawns) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		if (APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr)
		{
			OutPlayerPawns.Add(PlayerPawn);
		}
	}
}

void UFE_MonsterSpawnSubsystem::UpdateManagedMonsters(
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
	double CurrentTime)
{
	for (int32 Index = ManagedMonsters.Num() - 1; Index >= 0; --Index)
	{
		FFEManagedMonsterRuntime& Runtime = ManagedMonsters[Index];
		AFE_EnemyCharacter* Monster = Runtime.Monster.Get();
		if (!IsValid(Monster))
		{
			ManagedMonsters.RemoveAtSwap(Index);
			continue;
		}
		Runtime.SpawnCell = GetSpawnCell(Monster->GetActorLocation());

		if (Monster->IsDead())
		{
			if (Runtime.DeathStartTime < 0.0)
			{
				Runtime.DeathStartTime = CurrentTime;
				Monster->SetManagedSimulationActive(false);
			}
			if (CurrentTime - Runtime.DeathStartTime >= Runtime.DistanceSettings.CorpseLifetime)
			{
				RemoveManagedMonsterAt(Index, true);
			}
			continue;
		}

		const float NearestDistanceSquared = GetNearestPlayerDistanceSquared(
			Monster->GetActorLocation(), PlayerPawns);
		const float ActivationDistanceSquared = FMath::Square(
			FMath::Max(0.0f, Runtime.DistanceSettings.ActivationDistance));
		const bool bShouldSimulate = NearestDistanceSquared <= ActivationDistanceSquared;
		if (Runtime.bSimulationActive != bShouldSimulate)
		{
			Runtime.bSimulationActive = bShouldSimulate;
			Monster->SetManagedSimulationActive(bShouldSimulate);
		}

		if (bShouldSimulate)
		{
			Runtime.DormantStartTime = -1.0;
			continue;
		}

		if (Runtime.DormantStartTime < 0.0)
		{
			Runtime.DormantStartTime = CurrentTime;
		}
		const bool bEncounterMonster = Runtime.EncounterHandle != INDEX_NONE;
		const float DespawnDistanceSquared = FMath::Square(
			FMath::Max(Runtime.DistanceSettings.ActivationDistance, Runtime.DistanceSettings.DespawnDistance));
		if (!bEncounterMonster && NearestDistanceSquared > DespawnDistanceSquared &&
			CurrentTime - Runtime.DormantStartTime >= Runtime.DistanceSettings.DespawnGracePeriod)
		{
			RemoveManagedMonsterAt(Index, true);
		}
	}
}

void UFE_MonsterSpawnSubsystem::UpdateAmbientCells(
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
	double CurrentTime)
{
	if (!DefaultSpawnProfile || PlayerPawns.IsEmpty() || CurrentTime < NextAmbientEvaluationTime)
	{
		return;
	}
	NextAmbientEvaluationTime = CurrentTime + FMath::Max(0.1f, DefaultSpawnProfile->EvaluationInterval);

	const int32 DesiredCount = FMath::Min(
		DefaultSpawnProfile->MaxActiveEnemies,
		DefaultSpawnProfile->DesiredEnemiesPerPlayer * PlayerPawns.Num());
	const int32 SpawnCount = FMath::Min(
		FMath::Max(0, DesiredCount - CountAliveAmbientMonsters()),
		DefaultSpawnProfile->MaxSpawnsPerUpdate);
	if (SpawnCount <= 0)
	{
		return;
	}

	TArray<FIntPoint> AvailableCells;
	BuildAvailableSpawnCells(PlayerPawns, AvailableCells);
	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		if (!TrySpawnAmbientMonster(PlayerPawns, AvailableCells))
		{
			break;
		}
	}
}

void UFE_MonsterSpawnSubsystem::UpdateEncounters(
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
	double CurrentTime)
{
	for (int32 Index = ActiveEncounters.Num() - 1; Index >= 0; --Index)
	{
		FFEActiveMonsterEncounter& Encounter = ActiveEncounters[Index];
		if (!Encounter.Definition || !Encounter.Definition->Waves.IsValidIndex(Encounter.WaveIndex))
		{
			const int32 CompletedHandle = Encounter.Handle;
			ActiveEncounters.RemoveAtSwap(Index);
			OnEncounterCompleted.Broadcast(CompletedHandle);
			continue;
		}

		const FFEMonsterEncounterWave& Wave = Encounter.Definition->Waves[Encounter.WaveIndex];
		const int32 AliveCount = CountAliveMonstersForEncounter(Encounter.Handle);
		if (Encounter.SpawnedInWave >= Wave.TotalSpawnCount)
		{
			if (AliveCount == 0)
			{
				++Encounter.WaveIndex;
				Encounter.SpawnedInWave = 0;
				if (Encounter.Definition->Waves.IsValidIndex(Encounter.WaveIndex))
				{
					Encounter.NextSpawnTime = CurrentTime + FMath::Max(
						0.0f, Encounter.Definition->Waves[Encounter.WaveIndex].InitialDelay);
				}
			}
			continue;
		}

		if (AliveCount < FMath::Max(1, Wave.MaxAliveCount) && CurrentTime >= Encounter.NextSpawnTime &&
			TrySpawnEncounterMonster(Encounter, PlayerPawns))
		{
			++Encounter.SpawnedInWave;
			Encounter.NextSpawnTime = CurrentTime + FMath::Max(0.0f, Wave.SpawnInterval);
		}
	}
}

bool UFE_MonsterSpawnSubsystem::TrySpawnAmbientMonster(
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
	const TArray<FIntPoint>& AvailableCells)
{
	const FFEMonsterSpawnEntry* Entry = DefaultSpawnProfile
		? SelectWeightedEntry(DefaultSpawnProfile->MonsterEntries)
		: nullptr;
	if (!Entry || PlayerPawns.IsEmpty() || AvailableCells.IsEmpty())
	{
		return false;
	}

	FTransform SpawnTransform;
	if (!FindAmbientSpawnTransform(PlayerPawns, AvailableCells, SpawnTransform))
	{
		return false;
	}
	return SpawnManagedMonster(
		*Entry, SpawnTransform, DefaultSpawnProfile->RuntimeDistances, INDEX_NONE) != nullptr;
}

bool UFE_MonsterSpawnSubsystem::TrySpawnEncounterMonster(
	FFEActiveMonsterEncounter& Encounter,
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns)
{
	if (!Encounter.Definition || !Encounter.Definition->Waves.IsValidIndex(Encounter.WaveIndex))
	{
		return false;
	}
	const FFEMonsterEncounterWave& Wave = Encounter.Definition->Waves[Encounter.WaveIndex];
	const FFEMonsterSpawnEntry* Entry = SelectWeightedEntry(Wave.MonsterEntries);
	FTransform SpawnTransform;
	if (!Entry || !FindEncounterSpawnTransform(Encounter, SpawnTransform))
	{
		return false;
	}
	return SpawnManagedMonster(
		*Entry,
		SpawnTransform,
		Encounter.Definition->RuntimeDistances,
		Encounter.Handle,
		FindBestEncounterTarget(SpawnTransform.GetLocation(), PlayerPawns)) != nullptr;
}

AFE_EnemyCharacter* UFE_MonsterSpawnSubsystem::SpawnManagedMonster(
	const FFEMonsterSpawnEntry& SpawnEntry,
	const FTransform& SpawnTransform,
	const FFEMonsterRuntimeDistanceSettings& DistanceSettings,
	int32 EncounterHandle,
	APawn* EncounterTarget)
{
	UWorld* World = GetWorld();
	UClass* LoadedClass = SpawnEntry.EnemyClass.Get();
	if (!World || World->GetNetMode() == NM_Client || !LoadedClass)
	{
		RequestEnemyClassLoad(SpawnEntry.EnemyClass);
		return nullptr;
	}
	TSubclassOf<AFE_EnemyCharacter> EnemyClass(LoadedClass);
	const AFE_EnemyCharacter* DefaultEnemy = EnemyClass->GetDefaultObject<AFE_EnemyCharacter>();
	const UCapsuleComponent* DefaultCapsule = DefaultEnemy ? DefaultEnemy->GetCapsuleComponent() : nullptr;
	FTransform AdjustedSpawnTransform = SpawnTransform;
	if (DefaultCapsule)
	{
		FVector AdjustedLocation = AdjustedSpawnTransform.GetLocation();
		AdjustedLocation.Z += DefaultCapsule->GetScaledCapsuleHalfHeight();
		AdjustedSpawnTransform.SetLocation(AdjustedLocation);
	}
	if (IsSpawnSpaceBlocked(EnemyClass, AdjustedSpawnTransform.GetLocation()))
	{
		return nullptr;
	}

	AFE_EnemyCharacter* Monster = World->SpawnActorDeferred<AFE_EnemyCharacter>(
		EnemyClass,
		AdjustedSpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Monster)
	{
		return nullptr;
	}
	Monster->ConfigureSpawnManagement(
		DistanceSettings.VisualCullDistance,
		DistanceSettings.NetCullDistance);
	Monster->FinishSpawning(AdjustedSpawnTransform);

	FFEManagedMonsterRuntime& Runtime = ManagedMonsters.AddDefaulted_GetRef();
	Runtime.Monster = Monster;
	Runtime.SpawnCell = GetSpawnCell(Monster->GetActorLocation());
	Runtime.DistanceSettings = DistanceSettings;
	Runtime.EncounterHandle = EncounterHandle;
	Monster->SetManagedSimulationActive(true);
	if (EncounterHandle != INDEX_NONE && IsValid(EncounterTarget))
	{
		if (AFE_EnemyAIController* EnemyController = Cast<AFE_EnemyAIController>(Monster->GetController()))
		{
			EnemyController->SetEncounterCombatTarget(EncounterTarget);
		}
	}
	OnMonsterSpawned.Broadcast(Monster);
	return Monster;
}

bool UFE_MonsterSpawnSubsystem::FindAmbientSpawnTransform(
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
	const TArray<FIntPoint>& AvailableCells,
	FTransform& OutSpawnTransform) const
{
	if (!DefaultSpawnProfile || AvailableCells.IsEmpty())
	{
		return false;
	}
	const UFE_MonsterSpawnDeveloperSettings* Settings = GetDefault<UFE_MonsterSpawnDeveloperSettings>();
	const float CellSize = Settings ? FMath::Max(500.0f, Settings->SpawnCellSize) : 5000.0f;
	const float MinDistanceSquared = FMath::Square(FMath::Max(0.0f, DefaultSpawnProfile->MinSpawnDistance));
	const float MaxDistanceSquared = FMath::Square(FMath::Max(
		DefaultSpawnProfile->MinSpawnDistance, DefaultSpawnProfile->MaxSpawnDistance));
	for (int32 Attempt = 0; Attempt < DefaultSpawnProfile->LocationSearchAttempts; ++Attempt)
	{
		const FIntPoint Cell = AvailableCells[FMath::RandHelper(AvailableCells.Num())];
		FVector Candidate = GetSpawnCellCenter(Cell);
		Candidate.X += FMath::FRandRange(-0.5f, 0.5f) * CellSize;
		Candidate.Y += FMath::FRandRange(-0.5f, 0.5f) * CellSize;
		float NearestHorizontalDistanceSquared = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<APawn>& PlayerPointer : PlayerPawns)
		{
			if (const APawn* PlayerPawn = PlayerPointer.Get())
			{
				const FVector PlayerLocation = PlayerPawn->GetActorLocation();
				const float HorizontalDistanceSquared = FVector2D::DistSquared(
					FVector2D(Candidate), FVector2D(PlayerLocation));
				if (HorizontalDistanceSquared < NearestHorizontalDistanceSquared)
				{
					NearestHorizontalDistanceSquared = HorizontalDistanceSquared;
					Candidate.Z = PlayerLocation.Z;
				}
			}
		}

		const float NearestPlayerDistanceSquared = GetNearestPlayerDistanceSquared(Candidate, PlayerPawns);
		if (NearestPlayerDistanceSquared < MinDistanceSquared ||
			NearestPlayerDistanceSquared > MaxDistanceSquared)
		{
			continue;
		}
		if (ProjectSpawnCandidate(Candidate, OutSpawnTransform))
		{
			const FVector ProjectedLocation = OutSpawnTransform.GetLocation();
			const float ProjectedDistanceSquared = GetNearestPlayerDistanceSquared(ProjectedLocation, PlayerPawns);
			if (ProjectedDistanceSquared < MinDistanceSquared ||
				ProjectedDistanceSquared > MaxDistanceSquared ||
				(DefaultSpawnProfile->bAvoidVisibleSpawnLocations &&
				 IsVisibleToAnyPlayer(ProjectedLocation, DefaultSpawnProfile->VisibleSpawnHalfAngle)))
			{
				continue;
			}
			const FIntPoint ProjectedCell = GetSpawnCell(OutSpawnTransform.GetLocation());
			if (CountAliveMonstersInCell(ProjectedCell) <
				(Settings ? FMath::Max(1, Settings->MaxAliveEnemiesPerCell) : 4))
			{
				return true;
			}
		}
	}
	return false;
}

bool UFE_MonsterSpawnSubsystem::FindEncounterSpawnTransform(
	const FFEActiveMonsterEncounter& Encounter,
	FTransform& OutSpawnTransform) const
{
	if (!Encounter.Definition)
	{
		return false;
	}

	TArray<AFE_MonsterSpawnAnchor*, TInlineAllocator<8>> MatchingAnchors;
	const float SearchRadiusSquared = FMath::Square(FMath::Max(0.0f, Encounter.Definition->AnchorSearchRadius));
	for (const TWeakObjectPtr<AFE_MonsterSpawnAnchor>& AnchorPointer : SpawnAnchors)
	{
		AFE_MonsterSpawnAnchor* Anchor = AnchorPointer.Get();
		if (Anchor && Anchor->IsSpawnEnabled() && !Encounter.SpawnGroup.IsNone() &&
			Anchor->GetSpawnGroup() == Encounter.SpawnGroup &&
			FVector::DistSquared(Anchor->GetActorLocation(), Encounter.CenterLocation) <= SearchRadiusSquared)
		{
			MatchingAnchors.Add(Anchor);
		}
	}

	FVector Candidate = Encounter.CenterLocation;
	if (!MatchingAnchors.IsEmpty())
	{
		const AFE_MonsterSpawnAnchor* Anchor = MatchingAnchors[FMath::RandHelper(MatchingAnchors.Num())];
		const FVector2D Offset = FMath::RandPointInCircle(Anchor->GetSpawnRadius());
		Candidate = Anchor->GetActorLocation() + FVector(Offset.X, Offset.Y, 0.0f);
	}
	else
	{
		const FVector2D Offset = FMath::RandPointInCircle(Encounter.Definition->FallbackSpawnRadius);
		Candidate += FVector(Offset.X, Offset.Y, 0.0f);
	}
	return ProjectSpawnCandidate(Candidate, OutSpawnTransform);
}

bool UFE_MonsterSpawnSubsystem::ProjectSpawnCandidate(
	const FVector& CandidateLocation,
	FTransform& OutSpawnTransform) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	FNavLocation ProjectedLocation;
	if (!NavigationSystem || !NavigationSystem->ProjectPointToNavigation(
		CandidateLocation, ProjectedLocation, FVector(500.0f, 500.0f, 1000.0f)))
	{
		return false;
	}
	OutSpawnTransform = FTransform(FRotator(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f), ProjectedLocation.Location);
	return true;
}

bool UFE_MonsterSpawnSubsystem::IsVisibleToAnyPlayer(
	const FVector& SpawnLocation,
	float HalfAngleDegrees) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f)));
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		if (!PlayerController)
		{
			continue;
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector DirectionToSpawn = (SpawnLocation - ViewLocation).GetSafeNormal();
		if (FVector::DotProduct(ViewRotation.Vector(), DirectionToSpawn) < MinimumDot)
		{
			continue;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterSpawnVisibility), false);
		if (const APawn* PlayerPawn = PlayerController->GetPawn())
		{
			QueryParams.AddIgnoredActor(PlayerPawn);
		}
		FHitResult HitResult;
		const FVector TargetLocation = SpawnLocation + FVector(0.0f, 0.0f, 90.0f);
		const bool bBlocked = World->LineTraceSingleByChannel(
			HitResult, ViewLocation, TargetLocation, ECC_Visibility, QueryParams);
		if (!bBlocked || FVector::DistSquared(HitResult.ImpactPoint, TargetLocation) < FMath::Square(100.0f))
		{
			return true;
		}
	}
	return false;
}

bool UFE_MonsterSpawnSubsystem::IsSpawnSpaceBlocked(
	TSubclassOf<AFE_EnemyCharacter> EnemyClass,
	const FVector& SpawnLocation) const
{
	const UWorld* World = GetWorld();
	const AFE_EnemyCharacter* DefaultEnemy = EnemyClass ? EnemyClass->GetDefaultObject<AFE_EnemyCharacter>() : nullptr;
	const UCapsuleComponent* Capsule = DefaultEnemy ? DefaultEnemy->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule)
	{
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MonsterSpawnOverlap), false);
	return World->OverlapBlockingTestByProfile(
		SpawnLocation,
		FQuat::Identity,
		Capsule->GetCollisionProfileName(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
		QueryParams);
}

const FFEMonsterSpawnEntry* UFE_MonsterSpawnSubsystem::SelectWeightedEntry(
	const TArray<FFEMonsterSpawnEntry>& Entries) const
{
	float TotalWeight = 0.0f;
	for (const FFEMonsterSpawnEntry& Entry : Entries)
	{
		if (!Entry.EnemyClass.IsNull())
		{
			TotalWeight += FMath::Max(0.0f, Entry.Weight);
		}
	}
	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Selection = FMath::FRandRange(0.0f, TotalWeight);
	for (const FFEMonsterSpawnEntry& Entry : Entries)
	{
		if (Entry.EnemyClass.IsNull())
		{
			continue;
		}
		Selection -= FMath::Max(0.0f, Entry.Weight);
		if (Selection <= 0.0f)
		{
			return &Entry;
		}
	}
	return nullptr;
}

void UFE_MonsterSpawnSubsystem::RequestEnemyClassLoad(
	const TSoftClassPtr<AFE_EnemyCharacter>& EnemyClass)
{
	const FSoftObjectPath ClassPath = EnemyClass.ToSoftObjectPath();
	if (!ClassPath.IsValid() || EnemyClass.Get() || EnemyClassLoadHandles.Contains(ClassPath))
	{
		return;
	}
	TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ClassPath);
	EnemyClassLoadHandles.Add(ClassPath, LoadHandle);
}

float UFE_MonsterSpawnSubsystem::GetNearestPlayerDistanceSquared(
	const FVector& Location,
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns) const
{
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<APawn>& PlayerPointer : PlayerPawns)
	{
		if (const APawn* PlayerPawn = PlayerPointer.Get())
		{
			NearestDistanceSquared = FMath::Min(
				NearestDistanceSquared,
				FVector::DistSquared(Location, PlayerPawn->GetActorLocation()));
		}
	}
	return NearestDistanceSquared;
}

APawn* UFE_MonsterSpawnSubsystem::FindBestEncounterTarget(
	const FVector& Location,
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns) const
{
	APawn* BestPlayer = nullptr;
	int32 BestTargetLoad = TNumericLimits<int32>::Max();
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const UFE_AITargetCoordinatorSubsystem* TargetCoordinator = GetWorld()
		? GetWorld()->GetSubsystem<UFE_AITargetCoordinatorSubsystem>()
		: nullptr;
	for (const TWeakObjectPtr<APawn>& PlayerPointer : PlayerPawns)
	{
		APawn* PlayerPawn = PlayerPointer.Get();
		if (!PlayerPawn)
		{
			continue;
		}
		const int32 TargetLoad = TargetCoordinator
			? TargetCoordinator->GetTargetLoad(PlayerPawn)
			: 0;
		const float DistanceSquared = FVector::DistSquared(Location, PlayerPawn->GetActorLocation());
		if (TargetLoad < BestTargetLoad ||
			(TargetLoad == BestTargetLoad && DistanceSquared < BestDistanceSquared))
		{
			BestTargetLoad = TargetLoad;
			BestDistanceSquared = DistanceSquared;
			BestPlayer = PlayerPawn;
		}
	}
	return BestPlayer;
}

void UFE_MonsterSpawnSubsystem::BuildAvailableSpawnCells(
	const TArray<TWeakObjectPtr<APawn>>& PlayerPawns,
	TArray<FIntPoint>& OutAvailableCells) const
{
	const UFE_MonsterSpawnDeveloperSettings* Settings = GetDefault<UFE_MonsterSpawnDeveloperSettings>();
	const float CellSize = Settings ? FMath::Max(500.0f, Settings->SpawnCellSize) : 5000.0f;
	const int32 MaxEnemiesPerCell = Settings ? FMath::Max(1, Settings->MaxAliveEnemiesPerCell) : 4;
	const float MaxSpawnDistance = DefaultSpawnProfile
		? FMath::Max(DefaultSpawnProfile->MinSpawnDistance, DefaultSpawnProfile->MaxSpawnDistance)
		: 0.0f;
	const int32 CellRadius = FMath::CeilToInt(MaxSpawnDistance / CellSize) + 1;
	const float CellHalfDiagonal = CellSize * UE_SQRT_2 * 0.5f;
	const float CellSearchDistanceSquared = FMath::Square(MaxSpawnDistance + CellHalfDiagonal);
	TSet<FIntPoint> UniqueCells;

	for (const TWeakObjectPtr<APawn>& PlayerPointer : PlayerPawns)
	{
		const APawn* PlayerPawn = PlayerPointer.Get();
		if (!PlayerPawn)
		{
			continue;
		}
		const FVector PlayerLocation = PlayerPawn->GetActorLocation();
		const FIntPoint PlayerCell = GetSpawnCell(PlayerLocation);
		for (int32 OffsetX = -CellRadius; OffsetX <= CellRadius; ++OffsetX)
		{
			for (int32 OffsetY = -CellRadius; OffsetY <= CellRadius; ++OffsetY)
			{
				const FIntPoint CandidateCell(PlayerCell.X + OffsetX, PlayerCell.Y + OffsetY);
				FVector CellCenter = GetSpawnCellCenter(CandidateCell);
				CellCenter.Z = PlayerLocation.Z;
				if (FVector::DistSquared(CellCenter, PlayerLocation) <= CellSearchDistanceSquared)
				{
					UniqueCells.Add(CandidateCell);
				}
			}
		}
	}

	for (const FIntPoint& Cell : UniqueCells)
	{
		if (CountAliveMonstersInCell(Cell) < MaxEnemiesPerCell)
		{
			OutAvailableCells.Add(Cell);
		}
	}
}

FIntPoint UFE_MonsterSpawnSubsystem::GetSpawnCell(const FVector& WorldLocation) const
{
	const UFE_MonsterSpawnDeveloperSettings* Settings = GetDefault<UFE_MonsterSpawnDeveloperSettings>();
	const float CellSize = Settings ? FMath::Max(500.0f, Settings->SpawnCellSize) : 5000.0f;
	return FIntPoint(
		FMath::FloorToInt(WorldLocation.X / CellSize),
		FMath::FloorToInt(WorldLocation.Y / CellSize));
}

FVector UFE_MonsterSpawnSubsystem::GetSpawnCellCenter(const FIntPoint& Cell) const
{
	const UFE_MonsterSpawnDeveloperSettings* Settings = GetDefault<UFE_MonsterSpawnDeveloperSettings>();
	const float CellSize = Settings ? FMath::Max(500.0f, Settings->SpawnCellSize) : 5000.0f;
	return FVector(
		(static_cast<float>(Cell.X) + 0.5f) * CellSize,
		(static_cast<float>(Cell.Y) + 0.5f) * CellSize,
		0.0f);
}

int32 UFE_MonsterSpawnSubsystem::CountAliveAmbientMonsters() const
{
	int32 Count = 0;
	for (const FFEManagedMonsterRuntime& Runtime : ManagedMonsters)
	{
		const AFE_EnemyCharacter* Monster = Runtime.Monster.Get();
		if (Runtime.EncounterHandle == INDEX_NONE && Monster && !Monster->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

int32 UFE_MonsterSpawnSubsystem::CountAliveMonstersInCell(const FIntPoint& Cell) const
{
	int32 Count = 0;
	for (const FFEManagedMonsterRuntime& Runtime : ManagedMonsters)
	{
		const AFE_EnemyCharacter* Monster = Runtime.Monster.Get();
		if (Runtime.SpawnCell == Cell && Monster && !Monster->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

int32 UFE_MonsterSpawnSubsystem::CountAliveMonstersForEncounter(int32 EncounterHandle) const
{
	int32 Count = 0;
	for (const FFEManagedMonsterRuntime& Runtime : ManagedMonsters)
	{
		const AFE_EnemyCharacter* Monster = Runtime.Monster.Get();
		if (Runtime.EncounterHandle == EncounterHandle && Monster && !Monster->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

void UFE_MonsterSpawnSubsystem::RemoveManagedMonsterAt(int32 Index, bool bDestroyMonster)
{
	if (!ManagedMonsters.IsValidIndex(Index))
	{
		return;
	}
	AFE_EnemyCharacter* Monster = ManagedMonsters[Index].Monster.Get();
	ManagedMonsters.RemoveAtSwap(Index);
	if (IsValid(Monster))
	{
		OnMonsterDespawned.Broadcast(Monster);
		if (bDestroyMonster)
		{
			Monster->Destroy();
		}
	}
}
