// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildingViewModel.h"
#include "FEBuildingComponent.h"
#include "FEBuildingTypes.h"
#include "FEBuildingSettings.h"
#include "Engine/Texture2D.h"

// ---- 피스 엔트리 ----

void UFEBuildPieceEntryViewModel::Initialize(UFEBuildingComponent* InComponent, FPrimaryAssetId InPieceId, const FText& InDisplayName)
{
    Component = InComponent;
    UE_MVVM_SET_PROPERTY_VALUE(PieceId, InPieceId);
    UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InDisplayName);
}

void UFEBuildPieceEntryViewModel::SetSelected(bool bInSelected)
{
    UE_MVVM_SET_PROPERTY_VALUE(bSelected, bInSelected);
}

void UFEBuildPieceEntryViewModel::Select()
{
    UE_LOG(LogFEBuilding, Log, TEXT("Menu select: %s"), *PieceId.ToString());
    if (UFEBuildingComponent* Owner = Component.Get())
    {
        Owner->SelectPiece(PieceId);
        Owner->CloseBuildMenu();
    }
}

// ---- 투입 행 ----

void UFESupplyRowViewModel::Initialize(UFEBuildingComponent* InComponent, FGameplayTag InItemTag, const FText& InItemName)
{
    Component = InComponent;
    UE_MVVM_SET_PROPERTY_VALUE(ItemTag, InItemTag);
    UE_MVVM_SET_PROPERTY_VALUE(ItemName, InItemName);
}

void UFESupplyRowViewModel::SetCounts(int32 InSupplied, int32 InRequired)
{
    const FText NewLabel = FText::Format(NSLOCTEXT("FEBuilding", "SupplyRow", "{0}   {1} / {2}"),
        ItemName, FText::AsNumber(InSupplied), FText::AsNumber(InRequired));
    UE_MVVM_SET_PROPERTY_VALUE(Label, NewLabel);
    UE_MVVM_SET_PROPERTY_VALUE(bCanSupply, InSupplied < InRequired);
}

void UFESupplyRowViewModel::Supply()
{
    if (UFEBuildingComponent* Owner = Component.Get())
    {
        Owner->SupplyItem(ItemTag);
    }
}

// ---- 아이템 칸 ----

void UFEItemSlotViewModel::Initialize(UFEBuildingComponent* InComponent, bool bInIsStorageSlot, int32 InSlotIndex)
{
    Component = InComponent;
    bIsStorageSlot = bInIsStorageSlot;
    SlotIndex = InSlotIndex;
}

void UFEItemSlotViewModel::SetItem(FGameplayTag InItemTag, int32 InCount)
{
    Count = InCount;
    const UFEBuildingSettings* Settings = UFEBuildingSettings::Get();

    // ponytail: 아이콘은 작은 UI 텍스처라 동기 로드. 아이템 데이터(병일)가 오면 그쪽 비동기 로드로.
    UTexture2D* Texture = Settings->GetItemIcon(InItemTag).LoadSynchronous();
    FSlateBrush NewBrush;
    NewBrush.SetResourceObject(Texture);
    NewBrush.ImageSize = FVector2D(48.f, 48.f);

    UE_MVVM_SET_PROPERTY_VALUE(ItemTag, InItemTag);
    UE_MVVM_SET_PROPERTY_VALUE(ItemName, Settings->GetItemDisplayName(InItemTag));
    UE_MVVM_SET_PROPERTY_VALUE(IconBrush, NewBrush);
    UE_MVVM_SET_PROPERTY_VALUE(IconVisibility, Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    UE_MVVM_SET_PROPERTY_VALUE(NameVisibility, Texture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    UE_MVVM_SET_PROPERTY_VALUE(CountText, FText::AsNumber(InCount));
    UE_MVVM_SET_PROPERTY_VALUE(CountVisibility, ESlateVisibility::HitTestInvisible);
    UE_MVVM_SET_PROPERTY_VALUE(bIsFilled, true);
}

void UFEItemSlotViewModel::SetEmpty()
{
    Count = 0;
    UE_MVVM_SET_PROPERTY_VALUE(ItemTag, FGameplayTag());
    UE_MVVM_SET_PROPERTY_VALUE(ItemName, FText::GetEmpty());
    UE_MVVM_SET_PROPERTY_VALUE(IconBrush, FSlateBrush());
    UE_MVVM_SET_PROPERTY_VALUE(IconVisibility, ESlateVisibility::Collapsed);
    UE_MVVM_SET_PROPERTY_VALUE(NameVisibility, ESlateVisibility::Collapsed);
    UE_MVVM_SET_PROPERTY_VALUE(CountVisibility, ESlateVisibility::Collapsed);
    UE_MVVM_SET_PROPERTY_VALUE(bIsFilled, false);
}

void UFEItemSlotViewModel::Click()
{
    UFEBuildingComponent* Owner = Component.Get();
    if (Owner == nullptr || !bIsFilled)
    {
        return;
    }
    if (bIsStorageSlot)
    {
        Owner->TakeSlot(SlotIndex, ItemTag);
    }
    else
    {
        Owner->StoreStack(ItemTag, Count);
    }
}

// ---- HUD ----

void UFEBuildingViewModel::SetBuildMode(bool bInBuildMode)
{
    UE_MVVM_SET_PROPERTY_VALUE(bIsBuildMode, bInBuildMode);
    UE_MVVM_SET_PROPERTY_VALUE(BuildModeVisibility, bInBuildMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UFEBuildingViewModel::SetMenuOpen(bool bInOpen)
{
    UE_MVVM_SET_PROPERTY_VALUE(MenuVisibility, bInOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UFEBuildingViewModel::SetPieceEntries(const TArray<UObject*>& InEntries)
{
    UE_MVVM_SET_PROPERTY_VALUE(PieceEntries, TArray<TObjectPtr<UObject>>(InEntries));
}

void UFEBuildingViewModel::SetSelectedPieceName(const FText& InName)
{
    UE_MVVM_SET_PROPERTY_VALUE(SelectedPieceName, InName);
}

void UFEBuildingViewModel::SetSupplyPanelOpen(bool bInOpen)
{
    UE_MVVM_SET_PROPERTY_VALUE(SupplyPanelVisibility, bInOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UFEBuildingViewModel::SetSupplyTitle(const FText& InTitle)
{
    UE_MVVM_SET_PROPERTY_VALUE(SupplyTitle, InTitle);
}

void UFEBuildingViewModel::SetSupplyRows(const TArray<UObject*>& InRows)
{
    UE_MVVM_SET_PROPERTY_VALUE(SupplyRows, TArray<TObjectPtr<UObject>>(InRows));
}

void UFEBuildingViewModel::CloseMenu()
{
    if (UFEBuildingComponent* Owner = Component.Get())
    {
        Owner->CloseBuildMenu();
    }
}

void UFEBuildingViewModel::CloseSupplyPanel()
{
    if (UFEBuildingComponent* Owner = Component.Get())
    {
        Owner->CloseSupplyPanel();
    }
}

void UFEBuildingViewModel::SetStoragePanelOpen(bool bInOpen)
{
    UE_MVVM_SET_PROPERTY_VALUE(StoragePanelVisibility, bInOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UFEBuildingViewModel::SetStorageTitle(const FText& InTitle)
{
    UE_MVVM_SET_PROPERTY_VALUE(StorageTitle, InTitle);
}

void UFEBuildingViewModel::SetStorageSlots(const TArray<UObject*>& InSlots)
{
    UE_MVVM_SET_PROPERTY_VALUE(StorageSlots, TArray<TObjectPtr<UObject>>(InSlots));
}

void UFEBuildingViewModel::SetCarriedSlots(const TArray<UObject*>& InSlots)
{
    UE_MVVM_SET_PROPERTY_VALUE(CarriedSlots, TArray<TObjectPtr<UObject>>(InSlots));
}

void UFEBuildingViewModel::SetNotice(const FText& InText)
{
    UE_MVVM_SET_PROPERTY_VALUE(NoticeText, InText);
    UE_MVVM_SET_PROPERTY_VALUE(NoticeVisibility, InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UFEBuildingViewModel::CloseStoragePanel()
{
    if (UFEBuildingComponent* Owner = Component.Get())
    {
        Owner->CloseStoragePanel();
    }
}
