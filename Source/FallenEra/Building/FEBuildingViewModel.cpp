// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildingViewModel.h"
#include "FEBuildingComponent.h"
#include "FEBuildingTypes.h"

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