// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Building/FEBuildingTypes.h"
#include "FEBuildingSubsystem.generated.h"

class AFEBuildPiece;
class UFEBuildPieceDefinition;

/**
 * 구조 안정성(지지 거리) 계산과 붕괴 처리. [Server Only] 상태를 가지며, static 함수들은 클라/서버 공용.
 * 피스 간 연결은 저장하지 않고 재계산할 때마다 소켓 일치(5cm + 마주 봄)로 다시 구한다.
 * 스냅 판정(FEBuildingComponent)과 지지 그래프가 같은 FindConnectedPieces 를 쓰므로 둘이 어긋날 수 없다.
 */
UCLASS()
class FALLENERA_API UFEBuildingSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** 지지 거리 "anchor 에 닿지 않음" */
    static constexpr uint8 Unreachable = 255;

    static UFEBuildingSubsystem* Get(const UWorld* World);

    /** Point 반경 Radius 안의 배치된 피스들 (프리뷰는 NoCollision 이라 제외됨) */
    static void GatherNearbyPieces(const UWorld* World, const FVector& Point, float Radius, const AActor* IgnoreActor, TArray<AFEBuildPiece*>& OutPieces);

    /**
     * Transform 에 놓인 Piece 의 "붙는 쪽" 소켓과 5cm 이내에서 마주 보는 소켓을 가진 피스들.
     * 스냅 유효성과 지지 그래프 인접 관계가 모두 이 결과를 쓴다.
     */
    static void FindConnectedPieces(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FTransform& Transform, const AActor* IgnoreActor, TArray<AFEBuildPiece*>& OutConnected);

    /**
     * 이웃들로부터 예측한 지지 거리. anchor 피스는 0.
     * bBuiltOnly: true 면 완성된 이웃의 SupportDistance 만, false 면 모든 이웃의 DesignSupportDistance 를 본다.
     */
    static uint8 PredictSupportDistance(const UFEBuildPieceDefinition* Piece, const TArray<AFEBuildPiece*>& Connected, bool bBuiltOnly);

    /** Distance 가 이 피스 재료의 최대 지지 거리 이내인가 */
    static bool IsSupported(const UFEBuildPieceDefinition* Piece, uint8 Distance);

    /** [Server Only] 청사진이 지금 완성될 수 있는가 — 완성된 이웃만으로 지지되는가 */
    static bool CanComplete(const AFEBuildPiece* Piece);

    /** [Server Only] 피스가 BeginPlay/EndPlay 에서 호출 */
    void RegisterPiece(AFEBuildPiece* Piece);
    void UnregisterPiece(AFEBuildPiece* Piece);

    /** [Server Only] 다음 틱에 한 번 재계산. 한 프레임의 여러 변화를 모은다. */
    void MarkDirty();

    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    void Recalculate();

    /** 인접 그래프에서 anchor 기준 최단 지지 거리. bBuiltOnly 면 완성 피스만 노드로 쓴다. */
    void ComputeDistances(const TMap<AFEBuildPiece*, TArray<AFEBuildPiece*>>& Adjacency, bool bBuiltOnly, TMap<AFEBuildPiece*, uint8>& OutDistance) const;

    void CollapseNext();

    UPROPERTY(Transient)
    TArray<TObjectPtr<AFEBuildPiece>> Pieces;

    /** anchor 에서 먼 순서로 정렬된 붕괴 대기열 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<AFEBuildPiece>> CollapseQueue;

    FTimerHandle RecalculateTimer;
    FTimerHandle CollapseTimer;
    bool bIsCollapsing = false;
};