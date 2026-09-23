// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildingSubsystem.h"
#include "FEBuildBed.h"
#include "FEBuildInventoryProvider.h"
#include "FEBuildPiece.h"
#include "FEBuildPieceDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "HAL/IConsoleManager.h"
#include "FEBuildingSettings.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

namespace
{
    /** 소켓 일치로 인정하는 거리 */
    constexpr float SnapTolerance = 5.f;

    /** 두 소켓이 서로 마주 보는가 (X축이 반대 방향) */
    constexpr float FacingDotThreshold = -0.9f;

    /** 침구 위 리스폰 높이 */
    constexpr float RespawnHeight = 100.f;
}

// 세이브 인터페이스가 오기 전까지 쓰는 인메모리 스냅샷. 그때 이 전역과 명령 2개는 어댑터 호출로 바뀐다.
static TArray<FFEBuildPieceRecord> GBuildSnapshot;

static FAutoConsoleCommandWithWorld CmdBuildSnapshotSave(
    TEXT("fe.Build.SnapshotSave"),
    TEXT("Save all placed build pieces into an in-memory snapshot."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(World))
        {
            Subsystem->CollectRecords(GBuildSnapshot);
            UE_LOG(LogFEBuilding, Log, TEXT("Snapshot saved: %d piece(s)"), GBuildSnapshot.Num());
        }
    }));

static FAutoConsoleCommandWithWorld CmdBuildSnapshotLoad(
    TEXT("fe.Build.SnapshotLoad"),
    TEXT("Destroy current build pieces and restore the in-memory snapshot."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(World))
        {
            UE_LOG(LogFEBuilding, Log, TEXT("Snapshot load requested: %d piece(s)"), GBuildSnapshot.Num());
            Subsystem->RestoreRecords(GBuildSnapshot);
        }
    }));

UFEBuildingSubsystem* UFEBuildingSubsystem::Get(const UWorld* World)
{
    return World ? World->GetSubsystem<UFEBuildingSubsystem>() : nullptr;
}

bool UFEBuildingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

IFEBuildInventoryProvider* UFEBuildingSubsystem::FindInventoryProvider(const AActor* Actor)
{
    if (Actor == nullptr)
    {
        return nullptr;
    }
    const TArray<UActorComponent*> Providers = Actor->GetComponentsByInterface(UFEBuildInventoryProvider::StaticClass());
    return Providers.Num() > 0 ? Cast<IFEBuildInventoryProvider>(Providers[0]) : nullptr;
}

void UFEBuildingSubsystem::GatherNearbyPieces(const UWorld* World, const FVector& Point, float Radius, const AActor* IgnoreActor, TArray<AFEBuildPiece*>& OutPieces)
{
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(FEBuildGather), false, IgnoreActor);
    World->OverlapMultiByObjectType(Overlaps, Point, FQuat::Identity, FCollisionObjectQueryParams(ECC_FEBuildPiece), FCollisionShape::MakeSphere(Radius), Params);

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AFEBuildPiece* Piece = Cast<AFEBuildPiece>(Overlap.GetActor());
        const bool bIsPlaced = Piece != nullptr && Piece->GetDefinition() != nullptr && Piece->GetState() != EFEBuildPieceState::Preview && !Piece->IsActorBeingDestroyed();
        if (bIsPlaced)
        {
            OutPieces.AddUnique(Piece);
        }
    }
}

void UFEBuildingSubsystem::FindConnectedPieces(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FTransform& Transform, const AActor* IgnoreActor, TArray<AFEBuildPiece*>& OutConnected)
{
    if (World == nullptr || Piece == nullptr)
    {
        return;
    }
    const float SearchRadius = UFEBuildingSettings::Get()->SnapRadius;

    for (const FFEBuildSocket& PieceSocket : Piece->Sockets)
    {
        if (PieceSocket.AcceptTypes.IsEmpty())
        {
            continue; // 상대 전용 소켓은 붙는 쪽이 아니다
        }
        const FTransform PieceSocketWorld = PieceSocket.LocalTransform * Transform;
        const FVector PieceSocketForward = PieceSocketWorld.GetUnitAxis(EAxis::X);

        TArray<AFEBuildPiece*> Nearby;
        GatherNearbyPieces(World, PieceSocketWorld.GetLocation(), SearchRadius, IgnoreActor, Nearby);

        for (AFEBuildPiece* Target : Nearby)
        {
            const FTransform TargetXf = Target->GetActorTransform();
            for (const FFEBuildSocket& TargetSocket : Target->GetDefinition()->Sockets)
            {
                if (!PieceSocket.AcceptTypes.HasTag(TargetSocket.Type))
                {
                    continue;
                }
                const FTransform TargetSocketWorld = TargetSocket.LocalTransform * TargetXf;
                const bool bIsClose = FVector::DistSquared(PieceSocketWorld.GetLocation(), TargetSocketWorld.GetLocation()) <= SnapTolerance * SnapTolerance;
                const bool bIsFacing = FVector::DotProduct(PieceSocketForward, TargetSocketWorld.GetUnitAxis(EAxis::X)) < FacingDotThreshold;
                if (bIsClose && bIsFacing)
                {
                    OutConnected.AddUnique(Target);
                    break;
                }
            }
        }
    }
}

uint8 UFEBuildingSubsystem::PredictSupportDistance(const UFEBuildPieceDefinition* Piece, const TArray<AFEBuildPiece*>& Connected, bool bBuiltOnly)
{
    if (Piece == nullptr)
    {
        return Unreachable;
    }
    if (Piece->bCanPlaceOnGround)
    {
        return 0; // anchor
    }

    int32 Best = Unreachable;
    for (const AFEBuildPiece* Neighbor : Connected)
    {
        if (bBuiltOnly && Neighbor->GetState() != EFEBuildPieceState::Built)
        {
            continue;
        }
        const uint8 NeighborDistance = bBuiltOnly ? Neighbor->GetSupportDistance() : Neighbor->GetDesignSupportDistance();
        if (NeighborDistance == Unreachable)
        {
            continue;
        }
        Best = FMath::Min(Best, NeighborDistance + Piece->SupportCost);
    }
    return static_cast<uint8>(FMath::Min(Best, static_cast<int32>(Unreachable)));
}

bool UFEBuildingSubsystem::IsSupported(const UFEBuildPieceDefinition* Piece, uint8 Distance)
{
    if (Piece == nullptr || Distance == Unreachable)
    {
        return false;
    }
    return Distance <= UFEBuildingSettings::Get()->GetMaxSupportDistance(Piece->Material);
}

bool UFEBuildingSubsystem::CanComplete(const AFEBuildPiece* Piece)
{
    const UFEBuildPieceDefinition* Definition = Piece ? Piece->GetDefinition() : nullptr;
    if (Definition == nullptr)
    {
        return false;
    }
    TArray<AFEBuildPiece*> Connected;
    FindConnectedPieces(Piece->GetWorld(), Definition, Piece->GetActorTransform(), Piece, Connected);
    return IsSupported(Definition, PredictSupportDistance(Definition, Connected, true));
}

void UFEBuildingSubsystem::RegisterPiece(AFEBuildPiece* Piece)
{
    if (Piece == nullptr)
    {
        return;
    }
    Pieces.AddUnique(Piece);
    MarkDirty();
}

void UFEBuildingSubsystem::UnregisterPiece(AFEBuildPiece* Piece)
{
    Pieces.Remove(Piece);
    CollapseQueue.Remove(Piece);

    const UWorld* World = GetWorld();
    if (World && !World->bIsTearingDown)
    {
        MarkDirty();
    }
}

void UFEBuildingSubsystem::MarkDirty()
{
    UWorld* World = GetWorld();
    if (World == nullptr || RecalculateTimer.IsValid())
    {
        return; // 이미 예약됨
    }
    RecalculateTimer = World->GetTimerManager().SetTimerForNextTick(this, &UFEBuildingSubsystem::Recalculate);
}

FString UFEBuildingSubsystem::GetStablePlayerId(const APlayerState* PlayerState)
{
    // HT 가 AFallenEraPlayerState::GetStablePlayerId() 를 추가하면 아래 return 한 줄만 교체한다.
    //   return PlayerState ? Cast<AFallenEraPlayerState>(PlayerState)->GetStablePlayerId() : FString();
    // 그때까지는 플레이어 이름으로 대신한다 — 같은 세션 안에서만 확실히 유효하다.
    return PlayerState ? PlayerState->GetPlayerName() : FString();
}

void UFEBuildingSubsystem::SetRespawnBed(APlayerState* PlayerState, AFEBuildBed* Bed)
{
    const FString PlayerId = GetStablePlayerId(PlayerState);
    if (PlayerId.IsEmpty() || Bed == nullptr)
    {
        return;
    }

    // 플레이어당 침구 하나: 이전 침구 해제
    if (TObjectPtr<AFEBuildBed>* Previous = RespawnBeds.Find(PlayerId))
    {
        if (*Previous && *Previous != Bed)
        {
            (*Previous)->SetOwnerPlayerId(FString());
        }
    }
    // 침구당 주인 하나: 다른 플레이어가 쓰던 침구면 그쪽 등록 해제
    const FString OldOwnerId = Bed->GetOwnerPlayerId();
    if (!OldOwnerId.IsEmpty() && OldOwnerId != PlayerId)
    {
        RespawnBeds.Remove(OldOwnerId);
    }

    RespawnBeds.Add(PlayerId, Bed);
    Bed->SetOwnerPlayerId(PlayerId);
    UE_LOG(LogFEBuilding, Log, TEXT("Respawn bed set: %s -> %s"), *PlayerId, *Bed->GetName());

}

void UFEBuildingSubsystem::ClearRespawnBed(AFEBuildBed* Bed)
{
    if (Bed == nullptr)
    {
        return;
    }
    for (auto It = RespawnBeds.CreateIterator(); It; ++It)
    {
        if (It.Value() == Bed)
        {
            It.RemoveCurrent();
        }
    }
}

bool UFEBuildingSubsystem::GetRespawnTransform(const APlayerState* PlayerState, FTransform& OutTransform) const
{
    if (PlayerState == nullptr)
    {
        return false;
    }
    const TObjectPtr<AFEBuildBed>* Found = RespawnBeds.Find(GetStablePlayerId(PlayerState));
    const bool bHasBed = Found != nullptr && IsValid(*Found) && (*Found)->GetState() == EFEBuildPieceState::Built;
    if (!bHasBed)
    {
        return false;
    }
    OutTransform = (*Found)->GetActorTransform();
    OutTransform.AddToTranslation(FVector(0.f, 0.f, RespawnHeight));
    OutTransform.SetScale3D(FVector::OneVector);
    return true;
}

void UFEBuildingSubsystem::CollectRecords(TArray<FFEBuildPieceRecord>& OutRecords) const
{
    OutRecords.Reset();
    
    const UWorld* World = GetWorld();
    if (World == nullptr || World->GetNetMode() == NM_Client)
    {
        // [Server Only] Pieces 는 서버에서만 등록된다. 클라 창에서 부르면 0개가 나오므로 조용히 넘어가지 않는다.
        UE_LOG(LogFEBuilding, Warning, TEXT("CollectRecords: server only. Run this on the listen-server (host) window."));
        return;
    }
    
    for (const TObjectPtr<AFEBuildPiece>& Piece : Pieces)
    {
        // 정의가 아직 해석되지 않은 피스는 PieceId 만 남아 있어 복원 시 메시가 없다. 건너뛴다.
        if (IsValid(Piece) && Piece->GetDefinition() != nullptr)
        {
            Piece->WriteRecord(OutRecords.AddDefaulted_GetRef());
        }
    }
}

void UFEBuildingSubsystem::RestoreRecords(const TArray<FFEBuildPieceRecord>& Records)
{
    UWorld* World = GetWorld();
    if (World == nullptr || World->GetNetMode() == NM_Client)
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("RestoreRecords: server only. Run this on the listen-server (host) window."));
        return; // [Server Only] 클라이언트는 리플리케이션으로 따라온다
    }

    // 전체 교체. Destroy 가 UnregisterPiece 로 Pieces 를 건드리므로 복사본으로 순회한다.
    TArray<TObjectPtr<AFEBuildPiece>> Existing = Pieces;
    for (const TObjectPtr<AFEBuildPiece>& Piece : Existing)
    {
        if (IsValid(Piece))
        {
            Piece->Destroy();
        }
    }
    RespawnBeds.Reset();

    PendingRestore = Records;
    if (PendingRestore.Num() == 0)
    {
        return;
    }

    TArray<FPrimaryAssetId> Ids;
    for (const FFEBuildPieceRecord& Record : PendingRestore)
    {
        Ids.AddUnique(Record.PieceId);
    }

    // 이미 로드된 에셋뿐이면 핸들은 null 로 오고 델리게이트는 즉시 실행된다 (null 은 실패가 아님).
    RestoreHandle = UAssetManager::Get().LoadPrimaryAssets(
        Ids, { UFEBuildPieceDefinition::RuntimeBundle },
        FStreamableDelegate::CreateUObject(this, &UFEBuildingSubsystem::HandleRestoreAssetsLoaded));
}

void UFEBuildingSubsystem::HandleRestoreAssetsLoaded()
{
    RestoreHandle.Reset();
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    UAssetManager& AssetManager = UAssetManager::Get();
    int32 RestoredCount = 0;

    for (const FFEBuildPieceRecord& Record : PendingRestore)
    {
        const UFEBuildPieceDefinition* Definition = Cast<UFEBuildPieceDefinition>(AssetManager.GetPrimaryAssetObject(Record.PieceId));
        if (Definition == nullptr)
        {
            UE_LOG(LogFEBuilding, Warning, TEXT("Restore skipped: unknown piece %s"), *Record.PieceId.ToString());
            continue;
        }

        UClass* PieceClass = Definition->PieceClass.Get();
        const FTransform Transform(FRotator(0.f, Record.Yaw, 0.f), Record.Location);

        // 저장된 데이터는 이미 검증을 통과했던 배치다. ValidatePlacement 를 다시 돌리지 않는다
        // (복원 도중에는 이웃이 아직 안 생겨 스냅 검사가 전부 실패한다).
        AFEBuildPiece* Spawned = World->SpawnActorDeferred<AFEBuildPiece>(
            PieceClass ? PieceClass : AFEBuildPiece::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Spawned->InitializePiece(Definition, Record.State);
        Spawned->ReadRecord(Record);
        Spawned->FinishSpawning(Transform);

        if (AFEBuildBed* Bed = Cast<AFEBuildBed>(Spawned))
        {
            if (!Bed->GetOwnerPlayerId().IsEmpty())
            {
                RespawnBeds.Add(Bed->GetOwnerPlayerId(), Bed);
            }
        }
        ++RestoredCount;
    }

    PendingRestore.Reset();
    MarkDirty(); // 지지 거리는 전부 스폰된 뒤 한 번에 다시 계산
    UE_LOG(LogFEBuilding, Log, TEXT("Restored %d piece(s)"), RestoredCount);
}

void UFEBuildingSubsystem::Recalculate()
{
    RecalculateTimer.Invalidate();
    if (bIsCollapsing)
    {
        return; // 붕괴가 끝나면 CollapseNext 가 다시 MarkDirty 한다
    }

    Pieces.RemoveAll([](const TObjectPtr<AFEBuildPiece>& Piece)
    {
        return Piece == nullptr || Piece->IsActorBeingDestroyed() || Piece->GetDefinition() == nullptr;
    });

    // 인접 관계는 양방향. FindConnectedPieces 는 "붙는 쪽" 소켓 기준이라 토대 Top(상대 전용) 같은 연결은 반대편에서만 발견된다.
    // ponytail: 매번 전체 재계산. 500피스까지는 문제없고, 넘으면 영향 받은 서브그래프만 갱신한다.
    TMap<AFEBuildPiece*, TArray<AFEBuildPiece*>> Adjacency;
    for (AFEBuildPiece* Piece : Pieces)
    {
        Adjacency.FindOrAdd(Piece);
    }
    for (AFEBuildPiece* Piece : Pieces)
    {
        TArray<AFEBuildPiece*> Connected;
        FindConnectedPieces(GetWorld(), Piece->GetDefinition(), Piece->GetActorTransform(), Piece, Connected);
        for (AFEBuildPiece* Neighbor : Connected)
        {
            if (!Adjacency.Contains(Neighbor))
            {
                continue;
            }
            Adjacency[Piece].AddUnique(Neighbor);
            Adjacency[Neighbor].AddUnique(Piece);
        }
    }

    TMap<AFEBuildPiece*, uint8> DesignDistance;
    TMap<AFEBuildPiece*, uint8> BuiltDistance;
    ComputeDistances(Adjacency, false, DesignDistance);
    ComputeDistances(Adjacency, true, BuiltDistance);

    TArray<AFEBuildPiece*> ToCollapse;
    TArray<AFEBuildPiece*> ToComplete;
    for (AFEBuildPiece* Piece : Pieces)
    {
        const uint8 Design = DesignDistance.FindRef(Piece);
        const uint8* BuiltFound = BuiltDistance.Find(Piece);
        const uint8 Built = BuiltFound ? *BuiltFound : Unreachable;
        Piece->SetSupportDistances(Design, Built);

        const UFEBuildPieceDefinition* Definition = Piece->GetDefinition();
        if (Piece->GetState() == EFEBuildPieceState::Built)
        {
            if (!IsSupported(Definition, Built))
            {
                ToCollapse.Add(Piece);
            }
        }
        else if (Design == Unreachable)
        {
            // 토대가 사라져 설계 그래프에서도 끊긴 청사진. 다시 이어질 길이 없으니 Built 와 같이 무너뜨린다.
            ToCollapse.Add(Piece);
        }
        else if (Piece->IsFullySupplied())
        {
            const uint8 Predicted = PredictSupportDistance(Definition, Adjacency[Piece], true);
            if (IsSupported(Definition, Predicted))
            {
                ToComplete.Add(Piece);
            }
        }
    }

    if (ToCollapse.Num() > 0)
    {
        ToCollapse.Sort([](const AFEBuildPiece& A, const AFEBuildPiece& B)
        {
            return A.GetSupportDistance() > B.GetSupportDistance();
        });
        CollapseQueue = ToCollapse;
        bIsCollapsing = true;
        UE_LOG(LogFEBuilding, Log, TEXT("Structural collapse: %d piece(s)"), ToCollapse.Num());
        CollapseNext();
        return;
    }

    for (AFEBuildPiece* Piece : ToComplete)
    {
        Piece->TryComplete();
    }
}

void UFEBuildingSubsystem::ComputeDistances(const TMap<AFEBuildPiece*, TArray<AFEBuildPiece*>>& Adjacency, bool bBuiltOnly, TMap<AFEBuildPiece*, uint8>& OutDistance) const
{
    TArray<AFEBuildPiece*> Frontier;
    for (const TPair<AFEBuildPiece*, TArray<AFEBuildPiece*>>& Pair : Adjacency)
    {
        AFEBuildPiece* Piece = Pair.Key;
        if (bBuiltOnly && Piece->GetState() != EFEBuildPieceState::Built)
        {
            continue;
        }
        const bool bIsAnchor = Piece->IsAnchor();
        OutDistance.Add(Piece, bIsAnchor ? 0 : Unreachable);
        if (bIsAnchor)
        {
            Frontier.Add(Piece);
        }
    }

    // 정수 비용 다익스트라. 프론티어에서 거리 최소를 선형 탐색으로 꺼낸다.
    // ponytail: O(n^2) 최악. 기지 규모에선 무시 가능, 커지면 힙으로.
    while (Frontier.Num() > 0)
    {
        int32 BestIndex = 0;
        for (int32 Index = 1; Index < Frontier.Num(); ++Index)
        {
            if (OutDistance[Frontier[Index]] < OutDistance[Frontier[BestIndex]])
            {
                BestIndex = Index;
            }
        }
        AFEBuildPiece* Current = Frontier[BestIndex];
        Frontier.RemoveAtSwap(BestIndex);
        const uint8 CurrentDistance = OutDistance[Current];

        for (AFEBuildPiece* Neighbor : Adjacency[Current])
        {
            uint8* NeighborDistance = OutDistance.Find(Neighbor);
            if (NeighborDistance == nullptr)
            {
                continue;
            }
            const int32 Candidate = FMath::Min<int32>(CurrentDistance + Neighbor->GetSupportCost(), Unreachable);
            if (Candidate < *NeighborDistance)
            {
                *NeighborDistance = static_cast<uint8>(Candidate);
                Frontier.AddUnique(Neighbor);
            }
        }
    }
}

void UFEBuildingSubsystem::CollapseNext()
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    while (CollapseQueue.Num() > 0 && (CollapseQueue[0] == nullptr || CollapseQueue[0]->IsActorBeingDestroyed()))
    {
        CollapseQueue.RemoveAt(0);
    }

    if (CollapseQueue.Num() == 0)
    {
        bIsCollapsing = false;
        World->GetTimerManager().ClearTimer(CollapseTimer);
        MarkDirty();
        return;
    }

    AFEBuildPiece* Piece = CollapseQueue[0];
    CollapseQueue.RemoveAt(0);
    Piece->Collapse();

    const float Interval = UFEBuildingSettings::Get()->CollapseInterval;
    World->GetTimerManager().SetTimer(CollapseTimer, this, &UFEBuildingSubsystem::CollapseNext, FMath::Max(Interval, 0.01f), false);
}
