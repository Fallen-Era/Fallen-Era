#include "Combat/UI/Radial/FERadialMenuViewModel.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"

bool UFE_RadialMenuViewModel::SetItems(const TArray<FFE_RadialMenuItem>& InItems)
{
	TSet<FName> Ids;
	for (const auto& Item : InItems)
	{
		if (Item.Id.IsNone() || Ids.Contains(Item.Id)) { return false; }
		Ids.Add(Item.Id);
	}
	const FName PreviousId = Items.IsValidIndex(SelectedIndex) ? Items[SelectedIndex].Id : NAME_None;
	if (IconLoadHandle) { IconLoadHandle->CancelHandle(); IconLoadHandle.Reset(); }
	Items = InItems;
	SelectedIndex = Items.IndexOfByPredicate([PreviousId](const FFE_RadialMenuItem& Item) { return Item.Id == PreviousId && Item.bEnabled; });
	LoadedIcons.Reset();
	TArray<FSoftObjectPath> Paths;
	for (const auto& Item : Items)
	{
		if (UTexture2D* Icon = Item.Icon.Get()) { LoadedIcons.AddUnique(Icon); }
		else if (!Item.Icon.IsNull()) { Paths.AddUnique(Item.Icon.ToSoftObjectPath()); }
	}
	if (!Paths.IsEmpty())
	{
		IconLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
			FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingIcons));
	}
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Items);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedIndex);
	ValuesChanged.Broadcast();
	return true;
}

void UFE_RadialMenuViewModel::FinishLoadingIcons()
{
	for (const auto& Item : Items) { if (UTexture2D* Icon = Item.Icon.Get()) { LoadedIcons.AddUnique(Icon); } }
	IconLoadHandle.Reset();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Items);
	ValuesChanged.Broadcast();
}

void UFE_RadialMenuViewModel::SelectIndex(int32 Index)
{
	const int32 ValidIndex = Items.IsValidIndex(Index) && Items[Index].bEnabled ? Index : INDEX_NONE;
	if (SelectedIndex == ValidIndex) { return; }
	UE_MVVM_SET_PROPERTY_VALUE(SelectedIndex, ValidIndex);
	ValuesChanged.Broadcast();
}

void UFE_RadialMenuViewModel::BeginInteraction()
{
	bConfirmed = false;
	SelectIndex(INDEX_NONE);
}

bool UFE_RadialMenuViewModel::ConfirmSelection()
{
	if (bConfirmed || !Items.IsValidIndex(SelectedIndex) || !Items[SelectedIndex].bEnabled) { return false; }
	bConfirmed = true; // Bindings may synchronously close the menu or replace its items.
	const FFE_RadialMenuItem Item = Items[SelectedIndex];
	Item.OnExecute.ExecuteIfBound(Item.Id);
	OnItemExecuted.Broadcast(Item.Id);
	return true;
}

void UFE_RadialMenuViewModel::SetMenuText(FText InTitle, FText InConfirmHint, FText InCancelHint, FText InEmptyText)
{
	UE_MVVM_SET_PROPERTY_VALUE(Title, InTitle);
	UE_MVVM_SET_PROPERTY_VALUE(ConfirmHint, InConfirmHint);
	UE_MVVM_SET_PROPERTY_VALUE(CancelHint, InCancelHint);
	UE_MVVM_SET_PROPERTY_VALUE(EmptyText, InEmptyText);
	ValuesChanged.Broadcast();
}

void UFE_RadialMenuViewModel::Shutdown()
{
	if (IconLoadHandle) { IconLoadHandle->CancelHandle(); IconLoadHandle.Reset(); }
	LoadedIcons.Reset();
	BeginInteraction();
	SetItems({}); // Drop action targets when this menu interaction ends.
}

void UFE_RadialMenuViewModel::BeginDestroy()
{
	if (IconLoadHandle) { IconLoadHandle->CancelHandle(); IconLoadHandle.Reset(); }
	Super::BeginDestroy();
}

int32 UFE_RadialMenuViewModel::FindSector(FVector2D Offset, int32 Count, float InnerRadius, float OuterRadius)
{
	const double Distance = Offset.Size();
	if (Count <= 0 || Offset.ContainsNaN() || !FMath::IsFinite(InnerRadius) || !FMath::IsFinite(OuterRadius) ||
		InnerRadius < 0.0f || OuterRadius <= InnerRadius || Distance < InnerRadius || Distance > OuterRadius) { return INDEX_NONE; }
	double Angle = FMath::Atan2(Offset.X, -Offset.Y);
	if (Angle < 0.0) { Angle += UE_TWO_PI; }
	return FMath::Min(Count - 1, FMath::FloorToInt(Angle / UE_TWO_PI * Count));
}
