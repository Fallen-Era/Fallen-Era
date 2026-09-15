// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Building/FEBuildingTypes.h"
#include "FEBuildPieceDefinition.generated.h"

class AFEBuildPiece;
class UStaticMesh;
class UTexture2D;

/**
 * 배치 가능한 피스 1종의 정의 (바닥, 벽, 작업대 ...).
 * 피스 추가 = 이 에셋 1개 + (필요 시) AFEBuildPiece 의 BP 자식 1개. 코드 수정 없음.
 * Asset Manager 에 "BuildPiece" 타입으로 등록되며, 어디서나 FPrimaryAssetId 로 참조한다.
 */
UCLASS(BlueprintType)
class FALLENERA_API UFEBuildPieceDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Project Settings > Asset Manager > Primary Asset Types to Scan 의 타입명과 일치해야 함. */
    static const FPrimaryAssetType AssetType;

    /** 프리뷰/스폰에 필요한 에셋을 묶은 번들 이름. 이 번들만 로드하면 피스를 쓸 수 있다. */
    static const FName RuntimeBundle;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (Categories = "Build.Piece"))
    FGameplayTag Category;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> Icon;

    /** 스폰할 액터 클래스. 비어 있으면 AFEBuildPiece 자체. 기능성 가구는 서브클래스를 지정. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (AssetBundles = "Runtime"))
    TSoftClassPtr<AFEBuildPiece> PieceClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (AssetBundles = "Runtime"))
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Cost")
    EFEBuildMaterial Material = EFEBuildMaterial::Wood;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Cost")
    TArray<FFEBuildItemCost> RequiredItems;

    /** Built 피스 철거 시 RequiredItems 환불 비율. 청사진은 항상 100% 환불. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Cost", meta = (ClampMin = 0, ClampMax = 1))
    float RefundRate = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Durability", meta = (ClampMin = 1))
    float MaxHealth = 300.f;

    /** 이 피스가 얹힌 대상의 지지 거리에 더해지는 값. 바닥(토대)은 0 (anchor). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Structure", meta = (ClampMin = 0))
    int32 SupportCost = 1;

    /** 지형에 직접 배치 가능 (바닥, 모닥불, 스파이크). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement")
    bool bCanPlaceOnGround = false;

    /** 다른 피스의 소켓에 스냅해야만 배치 가능 (벽, 천장, 가구). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement")
    bool bRequiresSnap = true;

    /** 지형 배치 시 바닥 4코너와 지면 사이 허용 높이차 (cm). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement", meta = (EditCondition = "bCanPlaceOnGround", ClampMin = 0))
    float MaxGroundHeightDelta = 50.f;
};
