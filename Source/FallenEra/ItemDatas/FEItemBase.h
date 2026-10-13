// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FEItemBase.generated.h"

/**
 * FallenEraItemBase
 * - 기본적인 아이템 DA의 데이터들을 관리
 */


// ------------------------------------------------------------------------
// 비주얼 데이터 분리 구조체 
// (서버나 메모리에 무거운 텍스처/메시가 한 번에 올라가는 것을 방지)
// ------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FALLENERA_API FItemVisualData 
{
	GENERATED_BODY()
	
	/** 기본 UI 아이콘 (인벤토리 슬롯용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<class UTexture2D> Icon;

	/** 인벤토리 상세/상점용 썸네일 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<class UTexture2D> Thumbnail;
	
	/** 필드에 드랍되었을 때 렌더링할 바닥 메시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<class UStaticMesh> DropMesh;

	/** 
	 * 캐릭터가 장착했을 때 스폰/부착할 3D 메시 (둘 중 적합한 것 하나를 설정)
	 * - WorldSkeletalMesh: 활(시위 당김), 총기(노리쇠 후퇴), 망토/갑옷 등 뼈대(Bone)와 자체 애니메이션이 필요한 장비
	 * - WorldStaticMesh  : 단검, 도끼, 방패, 반지, 투구 등 뼈대가 필요 없는 단단한 고정 형태의 장비
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<class USkeletalMesh> WorldSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<class UStaticMesh> WorldStaticMesh;

	/** 장착 또는 사용 시 재생할 애니메이션 몽타주 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<class UAnimMontage> AnimationSet;
	
	// ----------------------------------------------------
	// VFX & SFX (시청각 효과)
	// ----------------------------------------------------
	// 필드에 떨어져 있을 때 재생할 이펙트 (예: 전설 등급의 황금빛 오라)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual|VFX")
	TSoftObjectPtr<class UNiagaraSystem> DropVFX;
	
	// 아이템을 루팅(줍기)할 때 재생할 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual|SFX")
	TSoftObjectPtr<class USoundBase> PickupSound;
	
	// 아이템을 인벤토리에서 버릴 때 재생할 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual|SFX")
	TSoftObjectPtr<class USoundBase> DropSound;
	
	// 아이템을 장착할 때 재생할 사운드 (철컥, 스윽 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual|SFX")
	TSoftObjectPtr<class USoundBase> EquipSound;
};

UCLASS(Abstract, BlueprintType)
class FALLENERA_API UFEItemBase : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 고유 ID (DB의 Primary Key 역할, Manager 등에서 검색 용도)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Base")
	FName ItemID;
	
	// 아이템 이름 (다국어 번역 지원을 위해 FString이 아닌 FText 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Base")
	FText ItemName;
	
	// 아이템 설명 (인게임 툴팁)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Base", meta=(MultiLine=true))
	FText Description;

	// 최대 중첩 수 (장비는 1 고정, 자원/소모품은 99 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Base", meta=(ClampMin="1"))
	int32 MaxStack = 1;

	// 아이템 개별 무게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Base", meta=(ClampMin="0.0"))
	float Weight = 0.0f;

	// 상점 구매가
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Economy", meta=(ClampMin="0"))
	int32 BuyPrice = 0;

	// 상점 판매가
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Economy", meta=(ClampMin="0"))
	int32 SellPrice = 0;

	// 캐릭터 장착 시 부착할 스켈레탈 메시 본/소켓 이름 (무기 손잡이, 머리 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Equip")
	FName EquipSocket;
	
	// 아이템 역할, 소재, 특성 등을 지정하는 태그 컨테이너
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Tags")
	FGameplayTagContainer ItemTags;
	
	// UI 이미지 및 월드에 떨어졌을 때의 3D 모델링 (소프트 참조 지연 로딩)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Visual")
	FItemVisualData Visuals;

	/** 언리얼 엔진 표준 Primary Asset Id 반환 (Type: "Item", ID: ItemID) */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Item"), ItemID.IsNone() ? GetFName() : ItemID);
	}
};
