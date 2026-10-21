#include "Combat/UI/FEAmmoSelectionViewModel.h"

#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Item/FEAmmoItemData.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"

void UFE_AmmoSelectionViewModel::Initialize(UFE_EquipmentComponent* InEquipment)
{
	Shutdown();
	SetMenuText(NSLOCTEXT("FEAmmo", "Title", "탄약 선택"), NSLOCTEXT("FEAmmo", "Confirm", "R 해제: 장전"),
		NSLOCTEXT("FEAmmo", "Cancel", "R 해제: 취소"), NSLOCTEXT("FEAmmo", "Empty", "호환 탄약 없음"));
	Equipment = InEquipment;
	if (Equipment.IsValid()) { Equipment->OnAmmoChanged.AddUniqueDynamic(this, &ThisClass::Refresh); }
	Refresh();
}

void UFE_AmmoSelectionViewModel::Shutdown()
{
	if (Equipment.IsValid()) { Equipment->OnAmmoChanged.RemoveDynamic(this, &ThisClass::Refresh); }
	Equipment.Reset();
	if (LoadHandle) { LoadHandle->CancelHandle(); LoadHandle.Reset(); }
	RequestedPaths.Reset();
	LoadedPresentation.Reset();
	ConfirmedAmmoId.Invalidate();
	Super::Shutdown();
}

void UFE_AmmoSelectionViewModel::Refresh()
{
	const FGuid PreviouslySelected = GetSelectedAmmoId();
	Options.Reset();
	TArray<FSoftObjectPath> Paths;
	if (Equipment.IsValid())
	{
		const FFE_WeaponAmmoState State = Equipment->GetAmmoState();
		for (const FFE_AmmoStack& Stack : Equipment->GetCompatibleAmmo())
		{
			FFE_AmmoSelectionOption& Option = Options.AddDefaulted_GetRef();
			Option.AmmoId = Stack.Instance.InstanceId;
			Option.Name = FText::FromName(Stack.AmmoType.GetTagName());
			Option.Count = Stack.Count;
			Option.DamageMultiplier = Stack.Instance.ResolveStats().DamageMultiplier;
			Option.bLoaded = State.LoadedAmmo > 0 && Stack.HasSameStats(State.LoadedAmmoData);
			if (UFE_AmmoItemData* Definition = Stack.AmmoData.Get())
			{
				LoadedPresentation.AddUnique(Definition);
				if (!Definition->Template.DisplayName.IsEmpty()) { Option.Name = Definition->Template.DisplayName; }
				Option.Icon = Definition->Template.Icon;
				if (UTexture2D* Icon = Option.Icon.Get()) { LoadedPresentation.AddUnique(Icon); }
				else if (!Option.Icon.IsNull()) { Paths.AddUnique(Option.Icon.ToSoftObjectPath()); }
			}
			else if (!Stack.AmmoData.IsNull()) { Paths.AddUnique(Stack.AmmoData.ToSoftObjectPath()); }
		}
	}
	SelectedIndex = Options.IndexOfByPredicate([PreviouslySelected](const FFE_AmmoSelectionOption& Option) { return Option.AmmoId == PreviouslySelected && Option.Count > 0; });
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Options);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedIndex);
	SyncMenuItems();
	Paths.RemoveAll([this](const FSoftObjectPath& Path) { return RequestedPaths.Contains(Path); });
	if (!Paths.IsEmpty())
	{
		for (const FSoftObjectPath& Path : Paths) { RequestedPaths.Add(Path); }
		LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::Refresh));
	}
}

void UFE_AmmoSelectionViewModel::SelectIndex(int32 Index)
{
	if (Items.Num() != Options.Num()) { SyncMenuItems(); }
	Super::SelectIndex(Index);
}

FGuid UFE_AmmoSelectionViewModel::GetSelectedAmmoId() const
{
	return Options.IsValidIndex(SelectedIndex) && Options[SelectedIndex].Count > 0 ? Options[SelectedIndex].AmmoId : FGuid();
}

void UFE_AmmoSelectionViewModel::SyncMenuItems()
{
	const int32 PreviousIndex = SelectedIndex;
	TArray<FFE_RadialMenuItem> MenuItems;
	for (const auto& Option : Options)
	{
		auto& Item = MenuItems.AddDefaulted_GetRef();
		Item.Id = FName(*Option.AmmoId.ToString(EGuidFormats::Digits));
		Item.Label = Option.Name;
		Item.SecondaryText = FText::Format(NSLOCTEXT("FEAmmo", "Count", "{0}발"), FText::AsNumber(Option.Count));
		Item.DetailText = FText::Format(NSLOCTEXT("FEAmmo", "Damage", "피해 ×{0}"), FText::AsNumber(Option.DamageMultiplier));
		Item.MarkerText = NSLOCTEXT("FEAmmo", "Loaded", "장전 중");
		Item.Icon = Option.Icon;
		Item.bEnabled = Option.Count > 0;
		Item.bMarked = Option.bLoaded;
		Item.OnExecute.BindDynamic(this, &ThisClass::ConfirmAmmoItem);
	}
	SetItems(MenuItems);
	Super::SelectIndex(PreviousIndex);
}

void UFE_AmmoSelectionViewModel::ConfirmAmmoItem(FName ItemId)
{
	ConfirmedAmmoId.Invalidate();
	const FFE_AmmoSelectionOption* Option = Options.FindByPredicate([ItemId](const FFE_AmmoSelectionOption& Entry)
	{
		return Entry.Count > 0 && FName(*Entry.AmmoId.ToString(EGuidFormats::Digits)) == ItemId;
	});
	if (Option) { ConfirmedAmmoId = Option->AmmoId; }
}
