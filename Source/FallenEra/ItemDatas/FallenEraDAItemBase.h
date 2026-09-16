// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FallenEraDAItemBase.generated.h"

/**
 * 
 */


// ------------------------------------------------------------------------
// 비주얼 데이터 분리 구조체 
// (서버나 메모리에 무거운 텍스처/메시가 한 번에 올라가는 것을 방지)
// ------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FItemVisualData 
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<class UTexture2D> Icon;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	TSoftObjectPtr<class UStaticMesh> DropMesh;
	
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

UCLASS()
class FALLENERA_API UFallenEraDAItemBase : public UPrimaryDataAsset
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
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Tags")
	FGameplayTagContainer ItemTags;
	
	// UI 이미지 및 월드에 떨어졌을 때의 3D 모델링 (소프트 참조 지연 로딩)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Visual")
	FItemVisualData Visuals;
};
