// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildingViewModel.h"
#include "FEBuildHudComponent.h"
#include "FEBuildingTypes.h"
#include "FEBuildingSettings.h"
#include "Engine/Texture2D.h"

// ---- 피스 엔트리 ----

void UFEBuildPieceEntryViewModel::Initialize(UFEBuildHudComponent* InComponent, FPrimaryAssetId InPieceId, const FText& InDisplayName)
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
    if (UFEBuildHudComponent* Owner = Component.Get())
    {
        Owner->SelectPieceFromMenu(PieceId);
    }
}

// ---- 투입 행 ----

void UFESupplyRowViewModel::Initialize(UFEBuildHudComponent* InComponent, FGameplayTag InItemTag, const FText& InItemName, const FText& InVariantName, bool bInShowOr)
{
    Component = InComponent;
    VariantName = InVariantName;
    UE_MVVM_SET_PROPERTY_VALUE(ItemTag, InItemTag);
    UE_MVVM_SET_PROPERTY_VALUE(ItemName, InItemName);
    UE_MVVM_SET_PROPERTY_VALUE(OrVisibility, bInShowOr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UFESupplyRowViewModel::SetCounts(int32 InSupplied, int32 InRequired, int32 InCarried)
{
    const FText Counts = FText::Format(NSLOCTEXT("FEBuilding", "SupplyRow", "{0}   {1} / {2}"),
        ItemName, FText::AsNumber(InSupplied), FText::AsNumber(InRequired));
    const FText NewLabel = VariantName.IsEmpty()
        ? Counts
        : FText::Format(NSLOCTEXT("FEBuilding", "SupplyRowVariant", "{0} — {1}"), VariantName, Counts);
    UE_MVVM_SET_PROPERTY_VALUE(Label, NewLabel);

    // 덜 찼고, 넣을 재료가 1개라도 있을 때만 (가진 만큼만 넣는 부분 투입 허용)
    UE_MVVM_SET_PROPERTY_VALUE(bCanSupply, InSupplied < InRequired && InCarried > 0);
}

void UFESupplyRowViewModel::Supply()
{
    if (UFEBuildHudComponent* Owner = Component.Get())
    {
        Owner->SupplyItem(ItemTag);
    }
}

// ---- 아이템 칸 ----

void UFEItemSlotViewModel::Initialize(UFEBuildHudComponent* InComponent, bool bInIsStorageSlot, int32 InSlotIndex)
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

void UFEItemSlotViewModel::SetEmpty(const FText& EmptyLabel)
{
    Count = 0;
    UE_MVVM_SET_PROPERTY_VALUE(ItemTag, FGameplayTag());
    UE_MVVM_SET_PROPERTY_VALUE(ItemName, EmptyLabel);
    UE_MVVM_SET_PROPERTY_VALUE(IconBrush, FSlateBrush());
    UE_MVVM_SET_PROPERTY_VALUE(IconVisibility, ESlateVisibility::Collapsed);
    UE_MVVM_SET_PROPERTY_VALUE(NameVisibility, EmptyLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    UE_MVVM_SET_PROPERTY_VALUE(CountVisibility, ESlateVisibility::Collapsed);
    UE_MVVM_SET_PROPERTY_VALUE(bIsFilled, false);
}

void UFEItemSlotViewModel::Click()
{
    UFEBuildHudComponent* Owner = Component.Get();
    if (Owner == nullptr || !bIsFilled) return;

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

void UFEBuildingViewModel::SetSupplyHint(const FText& InHint)
{
    UE_MVVM_SET_PROPERTY_VALUE(SupplyHint, InHint);
}

void UFEBuildingViewModel::SetSupplyRows(const TArray<UObject*>& InRows)
{
    UE_MVVM_SET_PROPERTY_VALUE(SupplyRows, TArray<TObjectPtr<UObject>>(InRows));
}

void UFEBuildingViewModel::CloseMenu()
{
    if (UFEBuildHudComponent* Owner = Component.Get())
    {
        Owner->CloseBuildMenu();
    }
}

void UFEBuildingViewModel::CloseSupplyPanel()
{
    if (UFEBuildHudComponent* Owner = Component.Get())
    {
        Owner->CloseSupplyPanel();
    }
}

void UFEBuildingViewModel::SetStoragePanelOpen(bool bInOpen)
{
    UE_MVVM_SET_PROPERTY_VALUE(StoragePanelVisibility, bInOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UFEBuildingViewModel::SetAmmoPanelOpen(bool bInOpen)
{
    UE_MVVM_SET_PROPERTY_VALUE(AmmoPanelVisibility, bInOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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

void UFEBuildingViewModel::SetFurnaceVisible(bool bInVisible)
{
    UE_MVVM_SET_PROPERTY_VALUE(FurnaceVisibility, bInVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UFEBuildingViewModel::SetFurnaceState(bool bInIsLit, float InProgress)
{
    UE_MVVM_SET_PROPERTY_VALUE(FurnaceProgress, InProgress);
    UE_MVVM_SET_PROPERTY_VALUE(FireButtonText, bInIsLit
        ? NSLOCTEXT("FEBuilding", "FireOff", "불 끄기")
        : NSLOCTEXT("FEBuilding", "FireOn", "불 켜기"));
}

void UFEBuildingViewModel::SetMenuTabTitle(const FText& InTitle)
{
    UE_MVVM_SET_PROPERTY_VALUE(MenuTabTitle, InTitle);
}

void UFEBuildingViewModel::SelectConstructionTab()
{
    if (UFEBuildHudComponent* Owner = Component.Get()) Owner->SelectMenuTab(EFEBuildMenuTab::Construction);
}

void UFEBuildingViewModel::SelectFurnitureTab()
{
    if (UFEBuildHudComponent* Owner = Component.Get()) Owner->SelectMenuTab(EFEBuildMenuTab::Furniture);
}

void UFEBuildingViewModel::SelectDefenseTab()
{
    if (UFEBuildHudComponent* Owner = Component.Get()) Owner->SelectMenuTab(EFEBuildMenuTab::Defense);
}

void UFEBuildingViewModel::SetAimedPiece(const FText& InName, float InHealthPercent, const FText& InRepairCost, const FText& InUpgrade)
{
    UE_MVVM_SET_PROPERTY_VALUE(AimedPieceVisibility, InName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (InName.IsEmpty()) return; // 숨길 때는 나머지 값을 건드리지 않는다 (10Hz 갱신에서 불필요한 알림 방지)

    UE_MVVM_SET_PROPERTY_VALUE(AimedPieceName, InName);
    UE_MVVM_SET_PROPERTY_VALUE(AimedHealthPercent, InHealthPercent);
    UE_MVVM_SET_PROPERTY_VALUE(AimedHealthText, FText::AsPercent(InHealthPercent));
    UE_MVVM_SET_PROPERTY_VALUE(RepairCostText, InRepairCost);
    UE_MVVM_SET_PROPERTY_VALUE(UpgradeText, InUpgrade);
    UE_MVVM_SET_PROPERTY_VALUE(UpgradeVisibility, InUpgrade.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UFEBuildingViewModel::SetBallistaHud(bool bInVisible, const FText& InText)
{
    UE_MVVM_SET_PROPERTY_VALUE(BallistaHudVisibility, bInVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (bInVisible)
    {
        UE_MVVM_SET_PROPERTY_VALUE(BallistaHudText, InText);
    }
}

void UFEBuildingViewModel::ToggleFurnaceLit()
{
    if (UFEBuildHudComponent* Owner = Component.Get())
    {
        Owner->ToggleFurnaceLit();
    }
}

void UFEBuildingViewModel::CloseStoragePanel()
{
    if (UFEBuildHudComponent* Owner = Component.Get())
    {
        Owner->CloseStoragePanel();
    }
}
