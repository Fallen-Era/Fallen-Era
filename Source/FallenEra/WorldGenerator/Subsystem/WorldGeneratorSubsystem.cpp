// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldGeneratorSubsystem.h"

#include "ParticleHelper.h"
#include "VectorTypes.h"
#include "Kismet/GameplayStatics.h"
#include "VoxelWorld.h"
#include "WorldGenerator/Data/Payload.h"
#include "Voronoi/Voronoi.h"
#include "VoxelPCG/Public/PCGApplyOnVoxelGraphSettings.h"
#include "WorldPartition/NavigationData/NavigationDataChunkActor.h"


namespace
{
	TArray<EBiomeType> GetBiomesInFixedOrder()
	{
		return {
			EBiomeType::TemperateForest,
			EBiomeType::Plains,
			EBiomeType::Hills,
			EBiomeType::Desert,
			EBiomeType::Tundra,
		};
	}
	
	FRegionGenerationSettings GetRegionGenrationSettings()
	{
		FRegionGenerationSettings Settings{};
		
		Settings.GridSpacing = 256.0;
		Settings.MaxSwapIterations = 64;
		
		Settings.bEnableWarp = false;
		Settings.WarpStrength = 32.0;
		Settings.WarpWavelength = 512.0;
		
		for (int32 Index = 0; Index << FBiomeAffinityTable::BiomeCount; ++Index)
		{
			Settings.Affinity.Values[Index][Index] = 1.0;
		}
		
		return Settings;
	}
	
	bool ValidateGenerationInput(const FWorldCreateRequest& Request, const FRegionGenerationSettings Settings)
	{
		const FBox2D& Bounds = Request.WorldBounds;
		
		if (!Bounds.bIsValid ||
			!FMath::IsFinite(Bounds.Min.X) ||
			!FMath::IsFinite(Bounds.Min.Y) ||
			!FMath::IsFinite(Bounds.Max.X) ||
			!FMath::IsFinite(Bounds.Max.Y))
		{
			return false;
		}
		
		if (!FMath::IsFinite(Settings.GridSpacing) ||
			Settings.GridSpacing <= 0.0 ||
			Settings.MaxSwapIterations < 0)
		{
			return false;
		}
		
		if (Settings.bEnableWarp && 
			(!FMath::IsFinite(Settings.WarpStrength) ||
			 !FMath::IsFinite(Settings.WarpWavelength) ||
			 Settings.WarpStrength < 0.0 ||
			 Settings.WarpWavelength < 0.0))
		{
			return false;
		}
		
		for (int32 A = 0; A < FBiomeAffinityTable::BiomeCount; ++A)
		{
			for (int32 B = 0; B < FBiomeAffinityTable::BiomeCount; ++B)
			{
				const double AB = Settings.Affinity.Values[A][B];
				const double BA = Settings.Affinity.Values[B][A];
				
				if (!FMath::IsFinite(AB) ||
					!FMath::IsNearlyEqual(AB, BA, 1.e-9))
				{
					return false;
				}
			}
		}
		
		return true;
	}
	
	struct FClippedRegionCell
	{
		TArray<FVector2D> Polygon;
		TArray<FRegionEdge> Edges;
	};
	
	bool PointLess(const FVector2D& A, const FVector2D& B)
	{
		return A.X != B.X ? A.X < B.X : A.Y < B.Y;
	}
	
	TArray<FClippedRegionCell> BuildClippedVoronoi(const TArray<FRegionSite>& Sites, const FBox2D& Bounds)
	{
		TArray<FClippedRegionCell> Result;
		
		if (Sites.IsEmpty())
		{
			return Result;
		}
		
		TArray<FVector> Points;
		Points.Reserve(Sites.Num());
		
		for (const FRegionSite& Site : Sites)
		{
			Points.Add(FVector(Site.Center.X, Site.Center.Y, 0.0));
		}
		
		const FBox Bounds3D(FVector(Bounds.Min.X, Bounds.Min.Y, -1.0), FVector(Bounds.Max.X, Bounds.Max.Y, 1.0));
		
		FVoronoiDiagram Diagram(Points, Bounds3D, 0.0);
		
		TArray<FVoronoiCellInfo> Cells;
		Diagram.ComputeAllCells(Cells);
		
		Result.SetNum(Cells.Num());
		
		for (int32 Region = 0; Region < Cells.Num(); ++Region)
		{
			const FVoronoiCellInfo& Cell = Cells[Region];
			FClippedRegionCell Output = Result[Region];
			
			// Faces [Vertex Num, Vertex Indices, Vertex Num, ...]
			int32 Cursor = 0;
			int32 FaceIndex = 0;
			
			while (Cursor < Cell.Faces.Num())
			{
				const int32 VertexCount = Cell.Faces[Cursor++];
				
				TArray<FVector2D> FacePoints;
				FacePoints.Reserve(VertexCount);
				
				for (int32 Vertex = 0; Vertex < VertexCount; ++Vertex)
				{
					const int32 VertexIndex = Cell.Faces[Cursor++];
					const FVector& Position = Cell.Vertices[VertexIndex];
					
					FacePoints.Add(FVector2D(Position.X, Position.Y));
				}
				
				const FVector& Normal = Cell.Normals[FaceIndex];
				const int32 Neighbor = Cell.Neighbors[FaceIndex];
				++FaceIndex;
				
				if (Normal.Z > 0.5)
				{
					Output.Polygon = MoveTemp(FacePoints);
					continue;
				}
				
				if (FMath::Abs(Normal.Z) >= 0.5)
				{
					continue;
				}
				
				// 옆면을 XY에 투영하면 선부이 된다.
				// 투영된 정점 중 가장 먼 두 점이 선분의 양 끝 점이다.
				FVector2D A = FVector2D::ZeroVector;
				FVector2D B = FVector2D::ZeroVector;
				double BestDistanceSquared = 0.0;
				
				for (int32 I = 0 ; I < FacePoints.Num(); ++I)
				{
					for (int32 J = I + 1; J < FacePoints.Num(); ++J)
					{
						const double DistanceSquared = FVector2D::DistSquared(FacePoints[I], FacePoints[J]);
						
						if (DistanceSquared > BestDistanceSquared)
						{
							BestDistanceSquared = DistanceSquared;
							A = FacePoints[I];
							B = FacePoints[J];
						}
					}
				}
				
				
				// 점 하나만 접하는 경우는 경계로 등록하지 않는다.
				if (BestDistanceSquared <= 1.e-12)
				{
					continue;
				}
				
				if (PointLess(B, A))
				{
					Swap(A,B);
				}
				
				FRegionEdge Edge;
				Edge.A = A;
				Edge.B = B;
				Edge.RegionA = Region;
				
				// 엔진에서 박스 외곽면은 음수 이웃 번호를 사용.
				 Edge.RegionB = Neighbor >= 0 ? Neighbor : INDEX_NONE;
				
				Output.Edges.Add(Edge);
			}
		}
		
		return Result;
	}
	
	bool ValidateCells(const TArray<FClippedRegionCell>& Cells, const int32 ExpectedCount)
	{
		if (ExpectedCount <= 0 || Cells.Num() != ExpectedCount)
		{
			return false;
		}
		
		for (int32 Region = 0; Region < Cells.Num(); ++Region)
		{
			const FClippedRegionCell& Cell = Cells[Region];
			
			if (Cell.Polygon.Num() < 3)
			{
				return false;
			}
			
			// 첫 정점을 원점으로 삼아 널빙르 계산.
			const FVector2D Origin = Cell.Polygon[0];
			double TwiceArea = 0.0;
			
			for (int32 I = 0; I < Cell.Polygon.Num(); ++I)
			{
				const FVector2D& P = Cell.Polygon[I];
				const FVector2D& Q = Cell.Polygon[(I + 1) % Cell.Polygon.Num()];
				
				if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y))
				{
					return false;
				}
				
				const FVector2D A = P - Origin;
				const FVector2D B = Q - Origin;
				
				TwiceArea += A.X * B.Y - A.Y * B.X;
			}
			if (!FMath::IsFinite(TwiceArea) || FMath::Abs(TwiceArea) <= 1.e-8)
			{
				return false;
			}
			
			for (const FRegionEdge& Edge : Cell.Edges)
			{
				if (Edge.RegionA != Region ||
					Edge.RegionB == Region ||
					(Edge.RegionB != INDEX_NONE && !Cells.IsValidIndex(Edge.RegionB)))
				{
					return false;
				}
			}
		}
		
		return true;
	}
	
	TArray<FRegionEdge> BuildUniqueEdges(const TArray<FClippedRegionCell>& Cells)
	{
		TArray<FRegionEdge> Result;
		
		for (int32 Region = 0; Region < Cells.Num(); ++Region)
		{
			for (const FRegionEdge& Edge : Cells[Region].Edges)
			{
				// 공유 경계는 작은 영역 번호 쪽에서만 등록한다.
				if (Edge.RegionB != INDEX_NONE && Region > Edge.RegionB)
				{
					continue;
				}
				
				Result.Add(Edge);
			}
		}
		
		return Result;
	}

	void SortEdgeDeterministically(TArray<FRegionEdge>& Edges)
	{
		Edges.Sort([](const FRegionEdge& A, const FRegionEdge& B)
		{
			if (A.RegionA != B.RegionA)
				return A.RegionA < B.RegionA;
			
			if (A.RegionB != B.RegionB)
				return A.RegionB < B.RegionB;
			
			if (A.A.X != B.A.X) return A.A.X < B.A.X;
			if (A.A.Y != B.A.Y) return A.A.Y < B.A.Y;
			if (A.B.X != B.B.X) return A.B.X < B.B.X;
			
			return A.B.Y < B.B.Y;
		});
	}
	
	void SortNeighborLinks(TArray<FRegionSite>& Sites)
	{
		for (FRegionSite& Site : Sites)
		{
			Site.Links.Sort([](const FRegionLink& A, const FRegionLink& B)
			{
				if (A.NeighborRegionIndex != B.NeighborRegionIndex)
				{
					return A.NeighborRegionIndex < B.NeighborRegionIndex;
				}
				
				return A.EdgeIndex < B.EdgeIndex;
			});
		}
	}
	
	double CalculateSwapGain(const int32 A, const int32 B, const FBiomeAffinityTable AffinityTable, const FGeneratedRegionData& Data)
	{
		TArray<int32> AffectedEdges;
		
		for (const FRegionLink& Link : Data.Sites[A].Links)
		{
			AffectedEdges.AddUnique(Link.EdgeIndex);
		}
		
		for (const FRegionLink& Link : Data.Sites[B].Links)
		{
			AffectedEdges.AddUnique(Link.EdgeIndex);
		}
		
		// 합산 순서 고정.
		AffectedEdges.Sort();
		
		const auto GetBiomeAfterSwap = [&](const int32 Region)
		{
			if (Region == A) return Data.Sites[B].Biome;
			if (Region == B) return Data.Sites[A].Biome;
			
			return Data.Sites[Region].Biome;
		};
		
		double Gain = 0.0;
		
		for (const int32 EdgeIndex : AffectedEdges)
		{
			const FRegionEdge& Edge = Data.Edges[EdgeIndex];
			
			if (Edge.RegionB == INDEX_NONE)
			{
				continue;
			}
			
			const double Length = FMath::Sqrt(FVector2D::DistSquared(Edge.A, Edge.B));
			
			const double Before = AffinityTable.Get(Data.Sites[Edge.RegionA].Biome, Data.Sites[Edge.RegionB].Biome);
			
			const double After = AffinityTable.Get(GetBiomeAfterSwap(Edge.RegionA), GetBiomeAfterSwap(Edge.RegionB));
			
			Gain += Length * (After - Before);
		}
		
		return Gain;
	}
	
}

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
	
	const auto Settings = GetRegionGenrationSettings();
	
	if (!ValidateGenerationInput(CreateParams, Settings))
		return false;
	
	const FVector2D Size = CreateParams.WorldBounds.GetSize();
	
	const double DesiredCount = 
		(Size.X / Settings.GridSpacing) * (Size.Y / Settings.GridSpacing);
	
	constexpr int32 MaxRegionCount = 1024;
	
	if (!FMath::IsFinite(DesiredCount) || DesiredCount > MaxRegionCount)
	{
		return false;
	}
	
	const int32 RegionCount = FMath::Max(1, FMath::CeilToInt(DesiredCount));
	
	ClearGenerator();
	
	if (!GenerateRegionSites(CreateParams.WorldBounds, CreateParams.WorldSeed, RegionCount))
	{
		ClearGenerator();
		return false;
	}
	
	FGeneratedRegionData Data;
	Data.Sites = MoveTemp(RegionSites);
	
	if (!GenerateVoronoiRegions(CreateParams.WorldBounds, Data))
	{
		ClearGenerator();
		return false;
	}
	
	FRandomStream BiomeStream(CreateParams.WorldSeed);
	AssignRandomBiomeTags(BiomeStream,Data);
	OptimizeBiomeAdjacency(Settings, Data);
	
	RegionSites = MoveTemp(Data.Sites);
	RegionEdges = MoveTemp(Data.Edges);
	
	ActiveGenerationSettings = Settings;
	
	FRandomStream WarpStream(CreateParams.WorldSeed);
	
	const auto MakePhase = [&WarpStream]()
	{
		// 난수 호출 순서를 명시적으로 고정.
		const double X = WarpStream.FRandRange(-10000.f, 10000.f);
		const double Y = WarpStream.FRandRange(-10000.f, 10000.f);
		
		return FVector2D(X, Y);
	};
	
	WarpPhaseX = MakePhase();
	WarpPhaseY = MakePhase();
	
	ValidateBiomeConnectivity();
	
	return true;
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

bool UWorldGeneratorSubsystem::GenerateVoronoiRegions(FBox2D Bounds, FGeneratedRegionData& Data)
{
	auto Cells = BuildClippedVoronoi(Data.Sites, Bounds);
	
	for (int32 Region = 0; Region < Cells.Num(); ++Region)
	{
		Data.Sites[Region].Polygon = MoveTemp(Cells[Region].Polygon);
		Data.Sites[Region].Links.Reset();
	}
	
	if (!ValidateCells(Cells, Data.Sites.Num()))
		return false;
	
	for (int32 Region = 0; Region < Cells.Num(); ++Region)
		Data.Sites[Region].Polygon = Cells[Region].Polygon;
	
	// 공유 경계는 한번만 등록.
	// 점 하나만 접하는 Cell은 이웃으로 취급하지 않음.
	Data.Edges = BuildUniqueEdges(Cells);
	
	SortEdgeDeterministically(Data.Edges);
	
	for (int32 EdgeIndex = 0; EdgeIndex < Data.Edges.Num(); ++EdgeIndex)
	{
		const auto& Edge = Data.Edges[EdgeIndex];
		
		if (Edge.RegionB == INDEX_NONE)
			continue;
		
		Data.Sites[Edge.RegionA].Links.Add( {Edge.RegionB, EdgeIndex });
		
		Data.Sites[Edge.RegionB].Links.Add( {Edge.RegionA, EdgeIndex });
	}
	
	SortNeighborLinks(Data.Sites);
	return true;
}

void UWorldGeneratorSubsystem::AssignRandomBiomeTags(FRandomStream& Stream, FGeneratedRegionData& Data)
{
	const auto Biomes = GetBiomesInFixedOrder();
	
	for (auto& Site : Data.Sites)
		Site.Biome = Biomes[Stream.RandRange(0, Biomes.Num() - 1)];
}

void UWorldGeneratorSubsystem::OptimizeBiomeAdjacency(const FRegionGenerationSettings& Settings, FGeneratedRegionData& Data)
{
	for (int32 Iter = 0; Iter < Settings.MaxSwapIterations; ++Iter)
	{
		int32 BestA = INDEX_NONE;
		int32 BestB = INDEX_NONE;
		double BestGain = 1e-9;
		
		for (int32 A = 0; A < Data.Sites.Num(); ++A)
		{
			for (int32 B = A + 1; B < Data.Sites.Num(); ++B)
			{
				if (Data.Sites[A].Biome == Data.Sites[B].Biome)
					continue;
				
				// A와 B에 연결된 경계의 점수 변화만 계산.
				// 대칭 친화도에서는 A-B 경계
				double Gain = CalculateSwapGain(A, B, Settings.Affinity, Data);
				
				if(Gain > BestGain + 1e-9)
				{
					BestGain = Gain;
					BestA = A;
					BestB = B;
				}
			}
		}
		if (BestA == INDEX_NONE)
			break;
		
		Swap(Data.Sites[BestA].Biome, Data.Sites[BestB].Biome);
	}
	
}

void UWorldGeneratorSubsystem::ValidateBiomeConnectivity()
{
	TArray<bool> Visited;
	Visited.Init(false, RegionSites.Num());
	
	int32 ComponentCounts[FBiomeAffinityTable::BiomeCount] = {};
	
	for (int32 Start = 0; Start < RegionSites.Num(); ++Start)
	{
		if (Visited[Start])
		{
			continue;
		}
		
		const EBiomeType Biome = RegionSites[Start].Biome;
		const int32 BiomeIndex = static_cast<int32>(Biome);
		
		++ComponentCounts[BiomeIndex];
		
		TArray<int32> Queue;
		Queue.Add(Start);
		Visited[Start] = true;
		
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Current = Queue[Head];
			
			for (const FRegionLink& Link : RegionSites[Current].Links)
			{
				const int32 Neighbor = Link.NeighborRegionIndex;
				
				if (!RegionSites.IsValidIndex(Neighbor) || Visited[Neighbor] || RegionSites[Neighbor].Biome != Biome)
				{
					continue;
				}
				
				Visited[Neighbor] = true;
				Queue.Add(Neighbor);
			}
		}
	}
	
	for (int32 Biome = 0; Biome < FBiomeAffinityTable::BiomeCount; ++Biome)
	{
		if (ComponentCounts[Biome] < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s::%s : Biome %d is split into %d connected components."),
				*GetClass()->GetName(), TEXT(__FUNCTION__),
				Biome,
				ComponentCounts[Biome]);
		}
	}
}

FVector2D UWorldGeneratorSubsystem::WarpOffset(const FVector2D& Position) const
{
	if (!ActiveGenerationSettings.bEnableWarp || ActiveGenerationSettings.WarpWavelength <= 0.0)
	{
		return FVector2D::ZeroVector;
	}
	
	const FVector2D Sample = Position / ActiveGenerationSettings.WarpWavelength;
	
	return FVector2D(
		FMath::PerlinNoise2D(Sample + WarpPhaseX),
		FMath::PerlinNoise2D(Sample + WarpPhaseY)
		) * ActiveGenerationSettings.WarpStrength;
}

int32 UWorldGeneratorSubsystem::FindNearestOriginalSite(const FVector2D& Position) const
{
	int32 BestRegion = INDEX_NONE;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	
	for (int32 Region = 0; Region < RegionSites.Num(); ++Region)
	{
		const double DistanceSquared = FVector2D::DistSquared(Position, RegionSites[Region].Center);
		
		// 같은 거리라면 먼저 발견한 작은 인덱스를 유지.
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestRegion = Region;
		}
	}
	
	return BestRegion;
}

void UWorldGeneratorSubsystem::ResetWarpState()
{
	ActiveGenerationSettings = FRegionGenerationSettings{};
	WarpPhaseX = FVector2D::ZeroVector;
	WarpPhaseY = FVector2D::ZeroVector;
}

EBiomeType UWorldGeneratorSubsystem::GetBiomeAt(FVector2D WorldPosition)
{
	if (!FMath::IsFinite(WorldPosition.X) || !FMath::IsFinite(WorldPosition.Y))
	{
		return EBiomeType::TemperateForest;
	}
	
	FVector2D OriginalPosition = 
		WorldPosition + WarpOffset(WorldPosition);
	
	int32 Region = FindNearestOriginalSite(OriginalPosition);
	
	return RegionSites.IsValidIndex(Region) ? RegionSites[Region].Biome : EBiomeType::TemperateForest;
}

void UWorldGeneratorSubsystem::ClearGenerator()
{
	RegionSites.Reset();
	RegionEdges.Reset();
	WorldWeightGrid.Reset();
	ResetWarpState();
}
