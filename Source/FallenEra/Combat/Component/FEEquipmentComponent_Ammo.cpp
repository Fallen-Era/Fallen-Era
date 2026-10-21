#include "Combat/Component/FEEquipmentComponent.h"

#include "Combat/Ability/FEWeaponReloadAbility.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/Item/FEAmmoItemData.h"
#include "Combat/UI/FEAmmoRadialWidget.h"
#include "Combat/UI/FEAmmoOptionWidget.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"

void UFE_EquipmentComponent::PreloadAmmoSelectionUI()
{
	if (GetNetMode() == NM_DedicatedServer || AmmoSelectionWidgetClass.IsNull() || AmmoUIHandle || LoadedAmmoOptionWidgetClass) { return; }
	if (AmmoSelectionWidgetClass.IsValid()) { FinishLoadingAmmoSelectionUI(); return; }
	AmmoUIHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(AmmoSelectionWidgetClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingAmmoSelectionUI));
}

void UFE_EquipmentComponent::FinishLoadingAmmoSelectionUI()
{
	if (bArmorEndingPlay) { return; }
	AmmoUIHandle.Reset();
	UClass* RootClass = AmmoSelectionWidgetClass.Get();
	if (!RootClass || RootClass->HasAnyClassFlags(CLASS_Abstract)) { return; }
	const auto* Defaults = RootClass->GetDefaultObject<UFE_AmmoRadialWidget>();
	if (Defaults->OptionWidgetClass.IsNull()) { return; }
	if (!Defaults->OptionWidgetClass.IsValid())
	{
		if (LoadedAmmoSelectionWidgetClass) { return; } // A failed async load must not retry recursively.
		LoadedAmmoSelectionWidgetClass = RootClass; // Retain root while its soft sector class loads.
		AmmoUIHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Defaults->OptionWidgetClass.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingAmmoSelectionUI));
		return;
	}
	LoadedAmmoOptionWidgetClass = Defaults->OptionWidgetClass.Get();
	LoadedAmmoSelectionWidgetClass = RootClass;
	AmmoSelectionUIReady.Broadcast();
}

void UFE_EquipmentComponent::InitializeAmmoInventory()
{
	if (bAmmoInventoryInitialized || AmmoLoadHandle || !GetOwner()->HasAuthority()) { return; }
	TArray<FSoftObjectPath> Paths;
	for (const FFE_AmmoStack& Seed : TestAmmoInventory)
	{
		if (!Seed.AmmoData.IsNull() && !Seed.AmmoData.IsValid()) { Paths.AddUnique(Seed.AmmoData.ToSoftObjectPath()); }
	}
	if (!Paths.IsEmpty())
	{
		AmmoLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
			FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingAmmoInventory));
		return;
	}
	FinishLoadingAmmoInventory();
}

void UFE_EquipmentComponent::FinishLoadingAmmoInventory()
{
	if (bArmorEndingPlay || bAmmoInventoryInitialized) { return; }
	bAmmoInventoryInitialized = true;
	for (FFE_AmmoStack Seed : TestAmmoInventory)
	{
		if (UFE_AmmoItemData* Definition = Seed.AmmoData.Get())
		{
			LoadedAmmoDefinitions.AddUnique(Definition);
			Seed.AmmoType = Definition->Template.AmmoType;
			if (Seed.bUseDefinitionStats) { Seed.Instance = Definition->Instance; }
		}
		else if (!Seed.AmmoData.IsNull()) { continue; }
		Seed.Instance.InstanceId.Invalidate(); // Shared defaults are seeds, not reusable inventory identities.
		StoreAmmoStack(Seed);
	}
	AmmoLoadHandle.Reset();
	if (CurrentWeaponData)
	{
		InitializeWeaponAmmo();
		if (CurrentWeaponInstance.LoadedAmmo == 0 && CanReloadWeapon()) { RequestReload(); }
	}
	PublishAmmoState();
}

bool UFE_EquipmentComponent::StoreAmmoStack(const FFE_AmmoStack& Input)
{
	const float Multiplier = Input.Instance.ResolveStats().DamageMultiplier;
	if (!Input.AmmoType.IsValid() || Input.Count < 0 || !FMath::IsFinite(Multiplier) || Multiplier < 0.0f) { return false; }
	FFE_AmmoStack* Existing = AmmoInventory.FindByPredicate([&Input](const FFE_AmmoStack& Entry) { return Entry.HasSameStats(Input); });
	if (Existing)
	{
		if (static_cast<int64>(Existing->Count) + Input.Count > MAX_int32) { return false; }
		Existing->Count += Input.Count;
	}
	else
	{
		FFE_AmmoStack Copy = Input;
		if (Copy.Instance.InstanceId.IsValid() && AmmoInventory.ContainsByPredicate([&Copy](const FFE_AmmoStack& Entry)
			{ return Entry.Instance.InstanceId == Copy.Instance.InstanceId; })) { return false; }
		if (!Copy.Instance.InstanceId.IsValid()) { Copy.Instance.InstanceId = FGuid::NewGuid(); }
		Copy.bUseDefinitionStats = false;
		AmmoInventory.Add(Copy);
	}
	return true;
}

bool UFE_EquipmentComponent::AddAmmoInstance(const FFE_AmmoStack& Stack)
{
	if (!GetOwner()->HasAuthority()) { return false; }
	InitializeAmmoInventory();
	if (!bAmmoInventoryInitialized) { return false; }
	FFE_AmmoStack Copy = Stack;
	if (!Copy.AmmoData.IsNull())
	{
		UFE_AmmoItemData* Definition = Copy.AmmoData.Get();
		if (!Definition) { return false; } // Inventory must preload definitions, never block a shot.
		Copy.AmmoType = Definition->Template.AmmoType;
		LoadedAmmoDefinitions.AddUnique(Definition);
	}
	if (!StoreAmmoStack(Copy)) { return false; }
	PublishAmmoState();
	return true;
}

int32 UFE_EquipmentComponent::GetReserveAmmo(FGameplayTag AmmoType) const
{
	int64 Total = 0;
	for (const FFE_AmmoStack& Stack : AmmoInventory) { if (Stack.AmmoType == AmmoType) { Total += Stack.Count; } }
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

TArray<FFE_AmmoStack> UFE_EquipmentComponent::GetCompatibleAmmo() const
{
	TArray<FFE_AmmoStack> Result;
	const FGameplayTag Type = GetOwner()->HasAuthority() && CurrentWeaponData
		? CurrentWeaponData->Template.AmmoSettings.AmmoType : AmmoState.AmmoType;
	for (const FFE_AmmoStack& Stack : AmmoInventory) { if (Stack.AmmoType == Type) { Result.Add(Stack); } }
	return Result;
}

bool UFE_EquipmentComponent::SetReserveAmmo(FGameplayTag AmmoType, int32 Count)
{
	if (!GetOwner()->HasAuthority() || !AmmoType.IsValid() || Count < 0) { return false; }
	InitializeAmmoInventory();
	if (!bAmmoInventoryInitialized) { return false; }
	// Tag-only integration affects ordinary x1 ammo, never erases generated variants.
	FFE_AmmoStack Ordinary;
	Ordinary.AmmoType = AmmoType;
	FFE_AmmoStack* Stack = AmmoInventory.FindByPredicate([&Ordinary](const FFE_AmmoStack& Entry) { return Entry.HasSameStats(Ordinary); });
	if (Stack) { Stack->Count = Count; }
	else { Ordinary.Count = Count; StoreAmmoStack(Ordinary); }
	if (CurrentWeaponData && !CurrentWeaponInstance.SelectedAmmoId.IsValid()) { InitializeWeaponAmmo(); }
	PublishAmmoState();
	return true;
}

void UFE_EquipmentComponent::SaveLoadedWeaponAmmo()
{
	if (GetOwner()->HasAuthority() && AmmoInstanceItemTag.IsValid())
	{
		// Test catalog has one weapon per tag. Real inventories supply individual InstanceIds.
		TestLoadedAmmoByItemTag.Add(AmmoInstanceItemTag, CurrentWeaponInstance);
	}
}

void UFE_EquipmentComponent::InitializeWeaponAmmo()
{
	if (!GetOwner()->HasAuthority() || !CurrentWeaponData) { return; }
	InitializeAmmoInventory();
	const FGameplayTag NewTag = GetCurrentItemTag();
	if (AmmoInstanceItemTag != NewTag)
	{
		if (const FFE_WeaponInstanceData* Saved = TestLoadedAmmoByItemTag.Find(NewTag)) { CurrentWeaponInstance = *Saved; }
		AmmoInstanceItemTag = NewTag;
	}
	const auto& Settings = CurrentWeaponData->Template.AmmoSettings;
	CurrentWeaponInstance.LoadedAmmo = FMath::Clamp(CurrentWeaponInstance.LoadedAmmo, 0, FMath::Max(1, Settings.MagazineCapacity));
	const bool bHasSelection = AmmoInventory.ContainsByPredicate([this](const FFE_AmmoStack& Stack)
	{
		return Stack.Instance.InstanceId == CurrentWeaponInstance.SelectedAmmoId;
	});
	if (Settings.bUsesAmmo && bAmmoInventoryInitialized && !bHasSelection)
	{
		if (!CurrentWeaponInstance.LoadedAmmoData.AmmoType.IsValid())
		{
			FFE_AmmoStack Ordinary;
			Ordinary.AmmoType = Settings.AmmoType;
			StoreAmmoStack(Ordinary);
			CurrentWeaponInstance.LoadedAmmoData = Ordinary;
		}
		FFE_AmmoStack Empty;
		static_cast<FFE_AmmoRoundData&>(Empty) = CurrentWeaponInstance.LoadedAmmoData;
		StoreAmmoStack(Empty);
		const FFE_AmmoStack* Selected = AmmoInventory.FindByPredicate([this](const FFE_AmmoStack& Stack)
		{
			return Stack.HasSameStats(CurrentWeaponInstance.LoadedAmmoData);
		});
		if (Selected)
		{
			CurrentWeaponInstance.SelectedAmmoId = Selected->Instance.InstanceId;
			CurrentWeaponInstance.LoadedAmmoData = *Selected;
		}
	}
	SaveLoadedWeaponAmmo();
	PublishAmmoState();
}

const FFE_AmmoStack* UFE_EquipmentComponent::FindSelectedAmmo() const
{
	const FGuid Id = RequestedReloadAmmoId.IsValid() ? RequestedReloadAmmoId : CurrentWeaponInstance.SelectedAmmoId;
	return AmmoInventory.FindByPredicate([Id](const FFE_AmmoStack& Stack) { return Stack.Instance.InstanceId == Id; });
}

void UFE_EquipmentComponent::PublishAmmoState()
{
	if (!GetOwner()->HasAuthority()) { return; }
	AmmoState.WeaponTag = CurrentWeaponData ? GetCurrentItemTag() : FGameplayTag();
	AmmoState.AmmoType = CurrentWeaponData ? CurrentWeaponData->Template.AmmoSettings.AmmoType : FGameplayTag();
	AmmoState.LoadedAmmo = CurrentWeaponData ? CurrentWeaponInstance.LoadedAmmo : 0;
	const FFE_AmmoStack* Selected = AmmoInventory.FindByPredicate([this](const FFE_AmmoStack& Stack) { return Stack.Instance.InstanceId == CurrentWeaponInstance.SelectedAmmoId; });
	AmmoState.ReserveAmmo = CurrentWeaponData && Selected ? Selected->Count : 0;
	AmmoState.WeaponInstanceId = CurrentWeaponData ? CurrentWeaponInstance.InstanceId : FGuid();
	AmmoState.SelectedAmmoId = CurrentWeaponData ? CurrentWeaponInstance.SelectedAmmoId : FGuid();
	AmmoState.LoadedAmmoData = CurrentWeaponData ? CurrentWeaponInstance.LoadedAmmoData : FFE_AmmoRoundData();
	OnAmmoChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
}

void UFE_EquipmentComponent::OnRep_AmmoState()
{
	if (AmmoState.bReloading)
	{
		if (UFE_WeaponAimingComponent* Aiming = GetOwner()->FindComponentByClass<UFE_WeaponAimingComponent>()) { Aiming->EndAim(); }
	}
	OnAmmoChanged.Broadcast();
}

bool UFE_EquipmentComponent::CanFireWeaponAmmo(const UFE_WeaponItemData* Weapon) const
{
	if (!Weapon || Weapon != CurrentWeaponData) { return false; }
	const auto& Settings = Weapon->Template.AmmoSettings;
	if (!Settings.bUsesAmmo) { return true; }
	if (!Settings.AmmoType.IsValid() || Settings.MagazineCapacity <= 0 || AmmoState.bReloading) { return false; }
	if (GetOwner()->HasAuthority() && CurrentWeaponInstance.LoadedAmmoData.AmmoType != Settings.AmmoType) { return false; }
	return GetOwner()->HasAuthority() ? CurrentWeaponInstance.LoadedAmmo > 0 :
		AmmoState.WeaponTag == GetCurrentItemTag() && AmmoState.LoadedAmmo > 0;
}

bool UFE_EquipmentComponent::ConsumeWeaponAmmo(const UFE_WeaponItemData* Weapon)
{
	FFE_AmmoRoundData Ignored;
	return ConsumeWeaponAmmo(Weapon, Ignored);
}

bool UFE_EquipmentComponent::ConsumeWeaponAmmo(const UFE_WeaponItemData* Weapon, FFE_AmmoRoundData& OutRound)
{
	OutRound = FFE_AmmoRoundData();
	if (!GetOwner()->HasAuthority() || !CanFireWeaponAmmo(Weapon)) { return false; }
	if (!Weapon->Template.AmmoSettings.bUsesAmmo) { return true; }
	OutRound = CurrentWeaponInstance.LoadedAmmoData;
	--CurrentWeaponInstance.LoadedAmmo;
	SaveLoadedWeaponAmmo();
	PublishAmmoState();
	return true;
}

void UFE_EquipmentComponent::RefundWeaponAmmo(const UFE_WeaponItemData* Weapon, FGuid InstanceId)
{
	RefundWeaponAmmo(Weapon, InstanceId, CurrentWeaponInstance.LoadedAmmoData);
}

void UFE_EquipmentComponent::RefundWeaponAmmo(const UFE_WeaponItemData* Weapon, FGuid InstanceId, const FFE_AmmoRoundData& Round)
{
	if (!GetOwner()->HasAuthority() || !Weapon || !Weapon->Template.AmmoSettings.bUsesAmmo) { return; }
	if (CurrentWeaponData == Weapon && CurrentWeaponInstance.InstanceId == InstanceId &&
		CurrentWeaponInstance.LoadedAmmoData.HasSameStats(Round) && CurrentWeaponInstance.LoadedAmmo < Weapon->Template.AmmoSettings.MagazineCapacity)
	{
		++CurrentWeaponInstance.LoadedAmmo;
		SaveLoadedWeaponAmmo();
	}
	else
	{
		FFE_AmmoStack Return;
		static_cast<FFE_AmmoRoundData&>(Return) = Round;
		Return.Count = 1;
		StoreAmmoStack(Return);
	}
	PublishAmmoState();
}

bool UFE_EquipmentComponent::CanReloadWeapon() const
{
	if (!CurrentWeaponData || AmmoState.bReloading || !UFE_CombatComponent::CanActorAttack(GetOwner())) { return false; }
	const auto& Settings = CurrentWeaponData->Template.AmmoSettings;
	const FFE_AmmoStack* Selected = FindSelectedAmmo();
	return Settings.bUsesAmmo && Settings.MagazineCapacity > 0 && Selected && Selected->Count > 0 &&
		Selected->AmmoType == Settings.AmmoType &&
		(!Selected->HasSameStats(CurrentWeaponInstance.LoadedAmmoData) || CurrentWeaponInstance.LoadedAmmo < Settings.MagazineCapacity);
}

bool UFE_EquipmentComponent::RequestReload()
{
	if (!GetOwner()->HasAuthority()) { return RequestReloadAmmo(AmmoState.SelectedAmmoId, AmmoState.WeaponInstanceId); }
	UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	return ASC && ASC->TryActivateAbilityByClass(UFE_WeaponReloadAbility::StaticClass());
}

bool UFE_EquipmentComponent::RequestReloadAmmo(FGuid AmmoId, FGuid WeaponInstanceId)
{
	if (!AmmoId.IsValid() || !WeaponInstanceId.IsValid()) { return false; }
	if (!GetOwner()->HasAuthority()) { ServerRequestReloadAmmo(AmmoId, WeaponInstanceId); return true; }
	if (CurrentWeaponInstance.InstanceId != WeaponInstanceId || AmmoState.bReloading) { return false; }
	RequestedReloadAmmoId = AmmoId;
	const bool bResult = CanReloadWeapon() && RequestReload();
	RequestedReloadAmmoId.Invalidate();
	return bResult;
}

bool UFE_EquipmentComponent::ServerRequestReloadAmmo_Validate(FGuid AmmoId, FGuid WeaponInstanceId)
{
	return AmmoId.IsValid() && WeaponInstanceId.IsValid();
}

void UFE_EquipmentComponent::ServerRequestReloadAmmo_Implementation(FGuid AmmoId, FGuid WeaponInstanceId)
{
	RequestReloadAmmo(AmmoId, WeaponInstanceId);
}

bool UFE_EquipmentComponent::BeginWeaponReload()
{
	if (!GetOwner()->HasAuthority() || !CanReloadWeapon()) { return false; }
	PendingReloadAmmoId = FindSelectedAmmo()->Instance.InstanceId;
	ReloadWeaponInstanceId = CurrentWeaponInstance.InstanceId;
	AmmoState.bReloading = true;
	PublishAmmoState();
	return true;
}

bool UFE_EquipmentComponent::FinishWeaponReload()
{
	if (!GetOwner()->HasAuthority() || !AmmoState.bReloading || !CurrentWeaponData ||
		ReloadWeaponInstanceId != CurrentWeaponInstance.InstanceId) { return false; }
	const int32 Index = AmmoInventory.IndexOfByPredicate([this](const FFE_AmmoStack& Stack) { return Stack.Instance.InstanceId == PendingReloadAmmoId; });
	if (!AmmoInventory.IsValidIndex(Index) || AmmoInventory[Index].Count <= 0 ||
		AmmoInventory[Index].AmmoType != CurrentWeaponData->Template.AmmoSettings.AmmoType) { CancelWeaponReload(); return false; }
	const FFE_AmmoRoundData NewRound = AmmoInventory[Index];
	const bool bChanging = !NewRound.HasSameStats(CurrentWeaponInstance.LoadedAmmoData);
	const int32 Kept = bChanging ? 0 : CurrentWeaponInstance.LoadedAmmo;
	const int32 Transfer = FMath::Min(AmmoInventory[Index].Count, FMath::Max(0, CurrentWeaponData->Template.AmmoSettings.MagazineCapacity - Kept));
	if (Transfer <= 0) { CancelWeaponReload(); return false; }
	FFE_AmmoStack OldRounds;
	static_cast<FFE_AmmoRoundData&>(OldRounds) = CurrentWeaponInstance.LoadedAmmoData;
	OldRounds.Count = bChanging ? CurrentWeaponInstance.LoadedAmmo : 0;
	const FFE_AmmoStack* ReturnStack = AmmoInventory.FindByPredicate([&OldRounds](const FFE_AmmoStack& Stack) { return Stack.HasSameStats(OldRounds); });
	if (OldRounds.Count > 0 && ReturnStack && static_cast<int64>(ReturnStack->Count) + OldRounds.Count > MAX_int32) { CancelWeaponReload(); return false; }
	// No yielding/publishing between these writes: commit the entire exchange at once.
	AmmoInventory[Index].Count -= Transfer;
	if (OldRounds.Count > 0) { StoreAmmoStack(OldRounds); }
	CurrentWeaponInstance.LoadedAmmoData = NewRound;
	CurrentWeaponInstance.LoadedAmmo = Kept + Transfer;
	CurrentWeaponInstance.SelectedAmmoId = PendingReloadAmmoId;
	PendingReloadAmmoId.Invalidate();
	ReloadWeaponInstanceId.Invalidate();
	AmmoState.bReloading = false;
	SaveLoadedWeaponAmmo();
	PublishAmmoState();
	return true;
}

void UFE_EquipmentComponent::CancelWeaponReload()
{
	if (!GetOwner()->HasAuthority()) { return; }
	PendingReloadAmmoId.Invalidate();
	ReloadWeaponInstanceId.Invalidate();
	AmmoState.bReloading = false;
	PublishAmmoState();
}

void UFE_EquipmentComponent::CompleteAmmoAttack(const UFE_WeaponItemData* Weapon, bool bCharged, bool bShotConsumed)
{
	if (!GetOwner()->HasAuthority() || CurrentWeaponData != Weapon || !Weapon || !Weapon->Template.AmmoSettings.bUsesAmmo || AmmoState.bReloading) { return; }
	if (bCharged)
	{
		if (bShotConsumed && BeginWeaponReload()) { FinishWeaponReload(); }
	}
	else if (CurrentWeaponInstance.LoadedAmmo == 0 && CanReloadWeapon()) { RequestReload(); }
}
