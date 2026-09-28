// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Components/SlateWrapperTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "Styling/SlateBrush.h"
#include "FEBuildingViewModel.generated.h"

class UFEBuildingComponent;

/** 빌드 메뉴의 피스 1줄. ListView 엔트리 위젯(WBP_PieceEntry)의 뷰모델 */
UCLASS(BlueprintType)
class FALLENERA_API UFEBuildPieceEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Initialize(UFEBuildingComponent* InComponent, FPrimaryAssetId InPieceId, const FText& InDisplayName);
	void SetSelected(bool bInSelected);

	/** [Client Only] 엔트리 버튼 클릭 → 컴포넌트에 선택 요청 */
	UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
	void Select();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FPrimaryAssetId PieceId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	bool bSelected = false;

private:
	TWeakObjectPtr<UFEBuildingComponent> Component;
	
};

/** 재료 투입 패널의 1줄 (RequiredItems 한 항목). WBP_SupplyRow 의 뷰모델 */
UCLASS(BlueprintType)
class FALLENERA_API UFESupplyRowViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
	void Initialize(UFEBuildingComponent* InComponent, FGameplayTag InItemTag, const FText& InItemName);
    void SetCounts(int32 InSupplied, int32 InRequired);

    /** [Client Only] "넣기" 버튼 → 서버에 이 항목 투입 요청 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void Supply();

    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    FGameplayTag ItemTag;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FText ItemName;

    /** "Item.Resource.Wood   3 / 10" — ponytail: 아이템 표시명은 아이템 담당 태그·데이터 확정 후 */
    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    FText Label;

    /** 아직 덜 찼으면 true → 버튼 IsEnabled */
    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    bool bCanSupply = false;

private:
    TWeakObjectPtr<UFEBuildingComponent> Component;
};

/**
 * 아이템 칸 1개. WBP_ItemSlot 의 뷰모델. 보관함 그리드와 소지품 그리드가 같이 쓴다.
 * 클릭 동작은 어느 그리드의 칸인지로 갈린다: 보관함 칸 = 꺼내기, 소지품 칸 = 넣기.
 */
UCLASS(BlueprintType)
class FALLENERA_API UFEItemSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** bInIsStorageSlot: 보관함 칸이면 true. InSlotIndex: 보관함 칸 번호 (소지품 칸은 INDEX_NONE) */
	void Initialize(UFEBuildingComponent* InComponent, bool bInIsStorageSlot, int32 InSlotIndex);
	void SetItem(FGameplayTag InItemTag, int32 InCount);
	void SetEmpty();

	/** [Client Only] 칸 클릭 → 보관함 칸이면 꺼내기, 소지품 칸이면 넣기 */
	UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
	void Click();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FGameplayTag ItemTag;

	/** 툴팁, 그리고 아이콘이 없을 때 칸 가운데 표시 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FText ItemName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FSlateBrush IconBrush;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	ESlateVisibility IconVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	ESlateVisibility NameVisibility = ESlateVisibility::Collapsed;

	/** 우하단 개수 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FText CountText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	ESlateVisibility CountVisibility = ESlateVisibility::Collapsed;

	/** 아이템이 들어 있는 칸. 빈 칸은 클릭 불가 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	bool bIsFilled = false;

private:
	TWeakObjectPtr<UFEBuildingComponent> Component;
	bool bIsStorageSlot = false;
	int32 SlotIndex = INDEX_NONE;
	int32 Count = 0;
};

/**
 * 건설 HUD 뷰모델. UFEBuildingComponent 가 소유하고 갱신한다. WBP_BuildHud 가 바인딩.
 * Visibility 를 직접 노출한다 — bool→Visibility 변환 함수를 위젯마다 다는 것보다 단순.
 */
UCLASS(BlueprintType)
class FALLENERA_API UFEBuildingViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    void SetBuildMode(bool bInBuildMode);
    void SetMenuOpen(bool bInOpen);
    void SetPieceEntries(const TArray<UObject*>& InEntries);
    void SetSelectedPieceName(const FText& InName);
    void SetSupplyPanelOpen(bool bInOpen);
    void SetSupplyTitle(const FText& InTitle);
    void SetSupplyRows(const TArray<UObject*>& InRows);
	void SetStoragePanelOpen(bool bInOpen);
	void SetStorageTitle(const FText& InTitle);
	void SetStorageSlots(const TArray<UObject*>& InSlots);
	void SetCarriedSlots(const TArray<UObject*>& InSlots);
	void SetNotice(const FText& InText); /** 잠깐 띄우는 안내 문구. 빈 텍스트면 숨김 */

	/** [Client Only] 저장고 패널 "닫기" 버튼 */
	UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
	void CloseStoragePanel();

    /** [Client Only] 메뉴 "닫기" 버튼 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void CloseMenu();

    /** [Client Only] 투입 패널 "닫기" 버튼 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void CloseSupplyPanel();

    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    bool bIsBuildMode = false;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    ESlateVisibility BuildModeVisibility = ESlateVisibility::Collapsed;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    ESlateVisibility MenuVisibility = ESlateVisibility::Collapsed;

    /** UFEBuildPieceEntryViewModel 배열. ListView.SetListItems 가 TArray<UObject*> 를 받아서 UObject 로 둔다 */
    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    TArray<TObjectPtr<UObject>> PieceEntries;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    FText SelectedPieceName;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    ESlateVisibility SupplyPanelVisibility = ESlateVisibility::Collapsed;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    FText SupplyTitle;

    /** UFESupplyRowViewModel 배열 */
    UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
    TArray<TObjectPtr<UObject>> SupplyRows;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	ESlateVisibility StoragePanelVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FText StorageTitle;

	/** 보관함 칸 (UFEItemSlotViewModel). 항상 Capacity 개. 빈 칸 포함 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	TArray<TObjectPtr<UObject>> StorageSlots;

	/** 내 소지품 칸 (UFEItemSlotViewModel) */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	TArray<TObjectPtr<UObject>> CarriedSlots;
	
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	FText NoticeText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Building|UI")
	ESlateVisibility NoticeVisibility = ESlateVisibility::Collapsed;

    TWeakObjectPtr<UFEBuildingComponent> Component;
};