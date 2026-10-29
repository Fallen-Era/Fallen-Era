// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FEBuildingTypes.h"
#include "FEBuildPieceDefinition.generated.h"

class AFEBuildPiece;
class UStaticMesh;
class UNiagaraSystem;
class USoundBase;
class UMaterialInterface;
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
    
	/** 재질 사슬: 자신부터 UpgradeTo 를 따라간 정의들 (예: 나무 → 돌 → 철). 투입 시 재질 선택·메뉴·프롬프트가 쓴다.
	 *  로드하지 않고 메모리에 있는 DA 만 따라간다 — UFEBuildingSubsystem 이 월드 시작 때 BuildPiece DA 전부를 미리 받아 둔다.
	 *  그 전(접속 직후 잠깐)에는 사슬이 짧게 나올 수 있다 */
    void GetMaterialChain(TArray<const UFEBuildPieceDefinition*>& OutChain) const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building")
    FText DisplayName;
	
	/** 재질과 무관한 이름 ("벽"). 재질 사슬의 시작 DA 에만 넣는다 — 빌드 메뉴와 재질 선택 패널 제목에 쓴다. 비우면 DisplayName */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building")
	FText BaseName;
	
	/** 빌드 메뉴 순서. 작은 값이 먼저, 같으면 에셋 이름순. 메뉴에는 재질 사슬의 시작 DA 만 나오므로 시작 DA 의 값만 쓰인다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building")
	int32 MenuOrder = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (Categories = "Build.Piece"))
    FGameplayTag Category;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> Icon;

    /** 스폰할 액터 클래스. 비어 있으면 AFEBuildPiece 자체. 기능성 가구는 서브클래스를 지정. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (AssetBundles = "Runtime"))
    TSoftClassPtr<AFEBuildPiece> PieceClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (AssetBundles = "Runtime"))
    TSoftObjectPtr<UStaticMesh> Mesh;
    
    /** 완성 상태 머티리얼. 같은 메시를 재질별 DA(나무/돌/철)가 공유하므로 색은 DA 가 정한다. 비우면 메시 기본 머티리얼 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (AssetBundles = "Runtime"))
    TSoftObjectPtr<UMaterialInterface> BuiltMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Cost")
    EFEBuildMaterial Material = EFEBuildMaterial::Wood;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Cost")
    TArray<FFEBuildItemCost> RequiredItems;

    /** Built 피스 철거 시 RequiredItems 환불 비율. 청사진은 항상 100% 환불. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Cost", meta = (ClampMin = 0, ClampMax = 1))
    float RefundRate = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Durability", meta = (ClampMin = 1))
    float MaxHealth = 300.f;
    
    /** 완성 피스가 사라질 때(파괴·붕괴·철거) 클라에서 재생하는 파편 효과. 비우면 없음. 재질별로 다르게 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Durability", meta = (AssetBundles = "Runtime"))
    TSoftObjectPtr<UNiagaraSystem> BreakEffect;

    /** BreakEffect 와 같은 시점의 소리. 비우면 없음 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Durability", meta = (AssetBundles = "Runtime"))
    TSoftObjectPtr<USoundBase> BreakSound;
    
    /** 망치 우클릭 업그레이드 대상 (다음 재질 DA). 비우면 마지막 단계. PieceClass 가 같아야 한다 (액터는 그대로, 정의만 교체).
     *  Runtime 번들이라 이 DA 를 로드하면 대상 DA 객체(이름·비용)도 같이 로드된다. 대상의 메시·머티리얼은 교체할 때 따로 로드 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Upgrade", meta = (AssetBundles = "Runtime"))
    TSoftObjectPtr<UFEBuildPieceDefinition> UpgradeTo;

    /** 이 피스가 얹힌 대상의 지지 거리에 더해지는 값. 바닥(토대)은 0 (anchor). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Structure", meta = (ClampMin = 0))
    int32 SupportCost = 1;

    /** 지형에 직접 배치 가능 (바닥, 모닥불, 스파이크). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement")
    bool bCanPlaceOnGround = false;

    /** 다른 피스의 소켓에 스냅해야만 배치 가능 (벽, 천장, 가구). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement")
    bool bRequiresSnap = true;
    
    /** 수직 아래(같은 격자 칸)에 토대가 있어야 배치 가능. 천장용 — 토대 범위 밖으로 뻗는 것을 막는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement", meta = (EditCondition = "bRequiresSnap"))
    bool bRequiresFoundationBelow = false;
    
    /** 지형뿐 아니라 완성된 구조물(토대·천장) 위에도 자유 배치 가능. 가구용. bCanPlaceOnGround 와 함께 쓴다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement", meta = (EditCondition = "bCanPlaceOnGround"))
    bool bCanPlaceOnStructure = false;

    /** 지형 배치 시 바닥 4코너와 지면 사이 허용 높이차 (cm). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement", meta = (EditCondition = "bCanPlaceOnGround", ClampMin = 0))
    float MaxGroundHeightDelta = 50.f;
	
	/** 함정처럼 칸으로 깔리는 피스의 칸 크기(cm). 0 이면 칸 배치 안 함 (함정 200).
	 *  칸은 메시 크기와 무관하다 — 메시는 칸보다 작아도 되고, 피벗이 바닥 가운데면 칸 중앙에 놓인다 (나중에 들어올 아트 메시 대비).
	 *  평평한 구조물(토대·천장) 위에서는 그 판의 칸에, 맨땅에서는 근처(SnapRadius) 칸 배치 피스에 이어 붙는다. 회전은 기준 + Q/E 90° */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement", meta = (EditCondition = "bCanPlaceOnGround", ClampMin = 0))
	float GridCellSize = 0.f;

	/** 바닥 4×4 지점 중 이 비율 이상이 MaxGroundHeightDelta 안에서 표면에 닿아야 배치 가능 (함정 0.8).
	 *  0 이면 기존 규칙(바닥 4코너 모두) — 가구는 0 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement", meta = (EditCondition = "bCanPlaceOnGround", ClampMin = 0, ClampMax = 1))
	float MinSupportCoverage = 0.f;
	
	/** 소켓 배치 때 "같은 자리 중복" 검사 상자 = 바운드 × 이 값.
	 *  0 이면 상자 대신 피벗 비교 — 새 피벗 5cm 안에 다른 피스의 피벗이 있으면 중복 (같은 소켓 두 번, 반대 방향 포함).
	 *  직각 삼각 벽처럼 바운드 중심이 모양 밖이라 어떤 크기로 줄여도 위에 얹힌 피스에 닿는 경우에 쓴다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Placement", meta = (ClampMin = 0, ClampMax = 1))
	float DuplicateCoreScale = 0.4f;
    
    /** 스냅 지점. 토대: Edge×4(붙는 쪽+상대) + Top×4(상대 전용). 벽: Bottom×1(붙는 쪽). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Snap")
    TArray<FFEBuildSocket> Sockets;
};
