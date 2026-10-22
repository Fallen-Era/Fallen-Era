// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldGeneratorSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "VoxelWorld.h"
#include "WorldGenerator/Data/Payload.h"
#include "Voronoi/Voronoi.h"

void UWorldGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	ClearGenerator();
}

bool UWorldGeneratorSubsystem::CreateWorld(const FWorldCreateRequest& CreateParams)
{
	// Registry Subsystem에서 Create World 책임 부분
	/*
	 * 
	 */
	
	
	const FBox2D WorldBound = CreateParams.WorldBounds;
	
	// if (GenerateRegionSeed())
	// {
	// 	return false;
	// }
	
	if (GenerateVoronoiRegions())
	{
		return false;
	}
	
	return false;
}

bool UWorldGeneratorSubsystem::GenerateRegionSites(
	const FBox2D& WorldBounds,
	const int32 StreamSeed,
	const int32 RegionCount)
{
	/*
	 * Voxel Sampling Default Size 100 
	 */
	
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : World is invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
	}
	
	AVoxelWorld* VoxelWorld = Cast<AVoxelWorld>(UGameplayStatics::GetActorOfClass(this, AVoxelWorld::StaticClass()));
	if (!IsValid(VoxelWorld))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : The Current World is not Voxel World."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return false;
	}
	
	if (RegionCount <= 0)
		return false;

	const FVector2D WorldSize = WorldBounds.GetSize();

	if (WorldSize.X <= 0.0 || WorldSize.Y <= 0.0)
		return false;

	RegionSites.Reset(RegionCount);

	FRandomStream Stream(StreamSeed);

	// 월드 종횡비에 맞춰 행 개수를 결정
	const int32 RowCount = FMath::Clamp(
		FMath::RoundToInt(
			FMath::Sqrt(
				static_cast<double>(RegionCount) *
				WorldSize.Y / WorldSize.X)),
		1, RegionCount);

	const int32 BaseColumns = RegionCount / RowCount;
	const int32 Remainder = RegionCount % RowCount;

	double CurrentY = WorldBounds.Min.Y;

	for (int32 Row = 0; Row < RowCount; ++Row)
	{
		const int32 Columns =
			BaseColumns + (Row < Remainder ? 1 : 0);

		// 각 셀이 동일한 면적을 갖도록 행 높이 조절
		const double CellWidth = WorldSize.X / Columns;
		const double CellHeight =
			WorldSize.Y * Columns / RegionCount;

		const double MaxY = (Row == RowCount - 1)
			? WorldBounds.Max.Y
			: CurrentY + CellHeight;

		for (int32 Col = 0; Col < Columns; ++Col)
		{
			const double MinX =
				WorldBounds.Min.X + Col * CellWidth;

			const double MaxX = (Col == Columns - 1)
				? WorldBounds.Max.X
				: MinX + CellWidth;

			FRegionSite Site;
			Site.Center = FVector2D(
				Stream.FRandRange(MinX, MaxX),
				Stream.FRandRange(CurrentY, MaxY));

			RegionSites.Add(Site);
		}

		CurrentY = MaxY;
	}

	return RegionSites.Num() == RegionCount;
}

bool UWorldGeneratorSubsystem::GenerateVoronoiRegions()
{
	
	return false;
}

void UWorldGeneratorSubsystem::AssignRandomBiomeTags()
{
	
	
}

void UWorldGeneratorSubsystem::ClearGenerator()
{
	RegionSites.Empty();
}
