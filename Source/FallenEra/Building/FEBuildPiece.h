// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "FEBuildingTypes.h"
#include "Interaction/FEInteractable.h"
#include "FEBuildPiece.generated.h"

class IFEBuildInventoryProvider;
class UFEBuildPieceDefinition;
class UMaterialInterface;
class UStaticMeshComponent;
struct FStreamableHandle;

/**
 * 배치된 구조물 1개. 같은 액터가 Preview -> Blueprint -> Built 로 상태만 바뀐다.
 * 리플리케이트되며 상태는 서버가 소유한다. 클라이언트는 PieceId + State + SuppliedCount + 지지 거리만 받고,
 * 정의 에셋에서 시각 요소를 스스로 복원한다.
 * 상호작용: 청사진이면 재료 투입, 완성품이면 서브클래스의 InteractBuilt (문·침구·작업대).
 */
UCLASS()
class FALLENERA_API AFEBuildPiece : public AActor, public IFEInteractable
{
    GENERATED_BODY()

public:
    AFEBuildPiece();

    /** [Server Only] (또는 로컬 프리뷰) SpawnActorDeferred 와 FinishSpawning 사이에 호출. 정의의 Runtime 번들이 로드되어 있어야 함. */
    void InitializePiece(const UFEBuildPieceDefinition* InDefinition, EFEBuildPieceState InState);

    /** [Server Only] 클라이언트는 OnRep_State 로 따라온다. */
    void SetState(EFEBuildPieceState NewState);
    
    /** [Server Only] ItemTag 항목에 인벤토리에서 있는 만큼 투입. 다 차고 지지되면 Built. 무언가 바뀌었으면 true. */
    bool TrySupply(IFEBuildInventoryProvider& Inventory, FGameplayTag ItemTag);
    
    /** [Server Only] 재료가 다 찬 청사진을 완성 시도. 지지 구조가 아직 청사진이면 false (대기). */
    bool TryComplete();

    /** [Server Only] 환불 후 제거. 청사진은 투입분 100%, Built 는 RefundRate. Inventory 가 없으면 환불 없이 제거. */
    void Demolish(IFEBuildInventoryProvider* Inventory);
    
    /** [Server Only] 지지를 잃어 무너진다. 환불 없음. */
    void Collapse();
    
    /** [Server Only] 서브시스템이 재계산 결과를 기록 */
    void SetSupportDistances(uint8 InDesignDistance, uint8 InBuiltDistance);

    /** [Client Only] 프리뷰 고스트 색상(유효/무효) 교체. 서브클래스는 추가 메시에도 적용 */
    virtual void SetPreviewValid(bool bIsValid);

    // IFEInteractable
    virtual bool CanInteract_Implementation(AActor* InstigatorActor) const override;
    virtual FText GetInteractText_Implementation(AActor* InstigatorActor) const override;
    virtual void Interact_Implementation(AActor* InstigatorActor) override;
    virtual void InteractLocal_Implementation(AActor* InstigatorActor) override;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    const UFEBuildPieceDefinition* GetDefinition() const;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    EFEBuildPieceState GetState() const;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    UStaticMeshComponent* GetMesh() const;
    
    /** 지형에 직접 놓이는 피스 = 지지의 출발점 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    bool IsAnchor() const;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    int32 GetSupportCost() const;

    /** 청사진+완성 전체 그래프 기준 지지 거리 (배치 예측용). 255 = 미도달 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    uint8 GetDesignSupportDistance() const;

    /** 완성 피스만의 그래프 기준 지지 거리 (완성 게이팅·붕괴용). 255 = 미도달 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    uint8 GetSupportDistance() const;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    int32 GetTotalRequired() const;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    bool IsFullySupplied() const;

    /** 0~1. 완성품은 1. UI 용 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    float GetSupplyProgress() const;

    /** RequiredItems[Index] 에 투입된 개수. 범위 밖이면 0 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    int32 GetSuppliedCount(int32 Index) const;

    /** 모든 항목의 투입 합 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    int32 GetTotalSupplied() const;

    /** [Native] 투입량 변화. 서버·클라 모두. 투입 패널이 행을 갱신할 때 쓴다 */
    FSimpleMulticastDelegate OnSupplyChangedNative;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    /** 완성 상태 상호작용 훅. 기본은 상호작용 없음. 문·침구·작업대가 오버라이드. */
    virtual bool CanInteractBuilt(AActor* InstigatorActor) const;
    virtual FText GetInteractTextBuilt(AActor* InstigatorActor) const;
    /** [Server Only] */
    virtual void InteractBuilt(AActor* InstigatorActor);
    /** [Client Only] */
    virtual void InteractBuiltLocal(AActor* InstigatorActor);

    /** ApplyState 끝에 호출. 서브클래스가 추가 메시(문짝 등)에 상태를 반영하는 자리 */
    virtual void OnApplyState(EFEBuildPieceState NewState);

    /** 메시의 모든 슬롯에 머티리얼 적용. Soft 가 비어 있으면 아무 것도 안 함 */
    static void ApplyMaterialToAll(UStaticMeshComponent* MeshComponent, const TSoftObjectPtr<UMaterialInterface>& SoftMaterial);

    /** 상태에 맞는 콜리전 프로파일 이름 (BuildPreview / BuildBlueprint / BuildPiece) */
    static FName GetCollisionProfileForState(EFEBuildPieceState PieceState);

    UPROPERTY(ReplicatedUsing = OnRep_PieceId)
    FPrimaryAssetId PieceId;

    UPROPERTY(ReplicatedUsing = OnRep_State)
    EFEBuildPieceState State = EFEBuildPieceState::Blueprint;
    
    /** RequiredItems 와 같은 길이. [i] = i 번째 재료에 투입된 개수 */
    UPROPERTY(ReplicatedUsing = OnRep_SuppliedCounts)
    TArray<int32> SuppliedCounts;
    
    UPROPERTY(Replicated)
    uint8 DesignSupportDistance = 255;

    UPROPERTY(Replicated)
    uint8 SupportDistance = 255;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UFUNCTION()
    void OnRep_PieceId();

    UFUNCTION()
    void OnRep_State();

    UFUNCTION()
    void OnRep_SuppliedCounts();

    /** BP 훅 (VFX/SFX 용). ApplyState 가 실행될 때마다 호출되며 최초 1회도 포함. */
    UFUNCTION(BlueprintImplementableEvent, Category = "FallenEra|Building")
    void OnStateChanged(EFEBuildPieceState NewState);

    /** BP 훅 (진행도 표시 용). 서버와 클라이언트 모두에서 호출. */
    UFUNCTION(BlueprintImplementableEvent, Category = "FallenEra|Building")
    void OnSupplyChanged(int32 NewSuppliedCount, int32 TotalRequired);

private:
    /** [Client Only] PieceId 를 로드된 정의로 해석한 뒤 ApplyState. */
    void RequestDefinition();
    void HandleDefinitionLoaded();

    /** 현재 상태에 맞는 메시/콜리전 프로파일/머티리얼 적용. 여러 번 호출해도 안전(멱등). */
    void ApplyState();

    /** PieceId 에서 해석됨(클라이언트) 또는 직접 전달됨(서버/프리뷰). 리플리케이트하지 않음. */
    UPROPERTY(Transient)
    TObjectPtr<const UFEBuildPieceDefinition> Definition;

    TSharedPtr<FStreamableHandle> DefinitionLoadHandle;
};
