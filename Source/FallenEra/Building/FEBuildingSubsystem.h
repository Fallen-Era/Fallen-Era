// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FEBuildingTypes.h"
#include "FEBuildingSubsystem.generated.h"

class AFEBuildBed;
class AFEBuildPiece;
class APlayerState;
class IFEBuildInventoryProvider;
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

    /** Actor 의 컴포넌트 중 IFEBuildInventoryProvider 구현체. 없으면 nullptr */
    static IFEBuildInventoryProvider* FindInventoryProvider(const AActor* Actor);

    /** Point 반경 Radius 안의 배치된 피스들 (프리뷰는 NoCollision 이라 제외됨) */
    static void GatherNearbyPieces(const UWorld* World, const FVector& Point, float Radius, const AActor* IgnoreActor, TArray<AFEBuildPiece*>& OutPieces);

    /** Transform 에 놓인 Piece 의 "붙는 쪽" 소켓과 5cm 이내에서 마주 보는 소켓을 가진 피스들 */
    static void FindConnectedPieces(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FTransform& Transform, const AActor* IgnoreActor, TArray<AFEBuildPiece*>& OutConnected);

    /** 이웃들로부터 예측한 지지 거리. anchor 피스는 0. bBuiltOnly: 완성 이웃의 SupportDistance 만 / 모든 이웃의 DesignSupportDistance */
    static uint8 PredictSupportDistance(const UFEBuildPieceDefinition* Piece, const TArray<AFEBuildPiece*>& Connected, bool bBuiltOnly);

    /** Distance 가 이 피스 재료의 최대 지지 거리 이내인가 */
    static bool IsSupported(const UFEBuildPieceDefinition* Piece, uint8 Distance);

    /** [Server Only] 청사진이 지금 완성될 수 있는가 — 완성된 이웃만으로 지지되는가 */
    static bool CanComplete(const AFEBuildPiece* Piece);

    /** [Server Only] 피스가 BeginPlay/EndPlay 에서 호출 */
    void RegisterPiece(AFEBuildPiece* Piece);
    void UnregisterPiece(AFEBuildPiece* Piece);

    /** [Server Only] 다음 틱에 한 번 재계산 */
    void MarkDirty();

    /** [Server Only] 플레이어의 리스폰 침구 지정. 이전 침구는 해제된다. */
    void SetRespawnBed(APlayerState* PlayerState, AFEBuildBed* Bed);

    /** [Server Only] 침구가 사라질 때 */
    void ClearRespawnBed(AFEBuildBed* Bed);

    /** 리스폰 위치. 침구가 없으면 false. 사망/리스폰 로직(HT)이 ChoosePlayerStart 에서 호출하는 연결 지점. */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    bool GetRespawnTransform(const APlayerState* PlayerState, FTransform& OutTransform) const;

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
    
    /** 플레이어별 리스폰 침구 */
    UPROPERTY(Transient)
    TMap<TObjectPtr<APlayerState>, TObjectPtr<AFEBuildBed>> RespawnBeds;

    FTimerHandle RecalculateTimer;
    FTimerHandle CollapseTimer;
    bool bIsCollapsing = false;
};