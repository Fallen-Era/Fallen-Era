#include "Combat/Component/FEEquipmentComponent.h"

#include "Combat/Ability/FEWeaponReloadAbility.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "GameFramework/Actor.h"

void UFE_EquipmentComponent::InitializeAmmoInventory()
{
	if (bAmmoInventoryInitialized || !GetOwner()->HasAuthority()) { return; }
	bAmmoInventoryInitialized = true;
	for (const FFE_AmmoStack& Seed : TestAmmoInventory)
	{
		if (!Seed.AmmoType.IsValid() || Seed.Count <= 0) { continue; }
		FFE_AmmoStack* Stack = AmmoInventory.FindByPredicate([&Seed](const FFE_AmmoStack& Entry) { return Entry.AmmoType == Seed.AmmoType; });
		if (!Stack) { Stack = &AmmoInventory.AddDefaulted_GetRef(); Stack->AmmoType = Seed.AmmoType; }
		Stack->Count = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Stack->Count) + Seed.Count));
	}
}

int32 UFE_EquipmentComponent::GetReserveAmmo(FGameplayTag AmmoType) const
{
	const FFE_AmmoStack* Stack = AmmoInventory.FindByPredicate([AmmoType](const FFE_AmmoStack& Entry) { return Entry.AmmoType == AmmoType; });
	return Stack ? Stack->Count : 0;
}

bool UFE_EquipmentComponent::SetReserveAmmo(FGameplayTag AmmoType, int32 Count)
{
	if (!GetOwner()->HasAuthority() || !AmmoType.IsValid() || Count < 0) { return false; }
	InitializeAmmoInventory();
	FFE_AmmoStack* Stack = AmmoInventory.FindByPredicate([AmmoType](const FFE_AmmoStack& Entry) { return Entry.AmmoType == AmmoType; });
	if (!Stack) { Stack = &AmmoInventory.AddDefaulted_GetRef(); Stack->AmmoType = AmmoType; }
	Stack->Count = Count;
	PublishAmmoState();
	return true;
}

void UFE_EquipmentComponent::SaveLoadedWeaponAmmo()
{
	if (GetOwner()->HasAuthority() && AmmoInstanceItemTag.IsValid())
	{
		TestLoadedAmmoByItemTag.Add(AmmoInstanceItemTag, CurrentWeaponInstance.LoadedAmmo);
	}
}

void UFE_EquipmentComponent::InitializeWeaponAmmo()
{
	if (!GetOwner()->HasAuthority() || !CurrentWeaponData) { return; }
	InitializeAmmoInventory();
	const FGameplayTag NewTag = GetCurrentItemTag();
	if (AmmoInstanceItemTag != NewTag)
	{
		if (const int32* Saved = TestLoadedAmmoByItemTag.Find(NewTag)) { CurrentWeaponInstance.LoadedAmmo = *Saved; }
		AmmoInstanceItemTag = NewTag;
	}
	CurrentWeaponInstance.LoadedAmmo = FMath::Clamp(CurrentWeaponInstance.LoadedAmmo, 0,
		FMath::Max(1, CurrentWeaponData->Template.AmmoSettings.MagazineCapacity));
	SaveLoadedWeaponAmmo();
	PublishAmmoState();
}

void UFE_EquipmentComponent::PublishAmmoState()
{
	if (!GetOwner()->HasAuthority()) { return; }
	AmmoState.WeaponTag = CurrentWeaponData ? GetCurrentItemTag() : FGameplayTag();
	AmmoState.AmmoType = CurrentWeaponData ? CurrentWeaponData->Template.AmmoSettings.AmmoType : FGameplayTag();
	AmmoState.LoadedAmmo = CurrentWeaponData ? CurrentWeaponInstance.LoadedAmmo : 0;
	AmmoState.ReserveAmmo = GetReserveAmmo(AmmoState.AmmoType);
	OnAmmoChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
}

void UFE_EquipmentComponent::OnRep_AmmoState()
{
	if (AmmoState.bReloading)
	{
		if (UFE_WeaponAimingComponent* Aiming = GetOwner()->FindComponentByClass<UFE_WeaponAimingComponent>()) { Aiming->EndAim(true); }
	}
	OnAmmoChanged.Broadcast();
}

bool UFE_EquipmentComponent::CanFireWeaponAmmo(const UFE_WeaponItemData* Weapon) const
{
	if (!Weapon || Weapon != CurrentWeaponData) { return false; }
	const FFE_WeaponAmmoSettings& Settings = Weapon->Template.AmmoSettings;
	if (!Settings.bUsesAmmo) { return true; }
	if (!Settings.AmmoType.IsValid() || Settings.MagazineCapacity <= 0 || AmmoState.bReloading) { return false; }
	return GetOwner()->HasAuthority() ? CurrentWeaponInstance.LoadedAmmo > 0 :
		AmmoState.WeaponTag == GetCurrentItemTag() && AmmoState.LoadedAmmo > 0;
}

bool UFE_EquipmentComponent::ConsumeWeaponAmmo(const UFE_WeaponItemData* Weapon)
{
	if (!GetOwner()->HasAuthority() || !CanFireWeaponAmmo(Weapon)) { return false; }
	if (!Weapon->Template.AmmoSettings.bUsesAmmo) { return true; }
	--CurrentWeaponInstance.LoadedAmmo;
	SaveLoadedWeaponAmmo();
	PublishAmmoState();
	return true;
}

void UFE_EquipmentComponent::RefundWeaponAmmo(const UFE_WeaponItemData* Weapon, FGuid InstanceId)
{
	if (!GetOwner()->HasAuthority() || !Weapon || CurrentWeaponData != Weapon ||
		CurrentWeaponInstance.InstanceId != InstanceId || !Weapon->Template.AmmoSettings.bUsesAmmo) { return; }
	if (CurrentWeaponInstance.LoadedAmmo < Weapon->Template.AmmoSettings.MagazineCapacity) { ++CurrentWeaponInstance.LoadedAmmo; }
	SaveLoadedWeaponAmmo();
	PublishAmmoState();
}

bool UFE_EquipmentComponent::CanReloadWeapon() const
{
	if (!CurrentWeaponData || AmmoState.bReloading || !UFE_CombatComponent::CanActorAttack(GetOwner())) { return false; }
	const FFE_WeaponAmmoSettings& Settings = CurrentWeaponData->Template.AmmoSettings;
	return Settings.bUsesAmmo && Settings.AmmoType.IsValid() && Settings.MagazineCapacity > 0 &&
		CurrentWeaponInstance.LoadedAmmo < Settings.MagazineCapacity && GetReserveAmmo(Settings.AmmoType) > 0;
}

bool UFE_EquipmentComponent::RequestReload()
{
	UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	// ServerOnly GA activation from an owning client uses GAS's existing activation RPC.
	return ASC && ASC->TryActivateAbilityByClass(UFE_WeaponReloadAbility::StaticClass());
}

bool UFE_EquipmentComponent::BeginWeaponReload()
{
	if (!GetOwner()->HasAuthority() || !CanReloadWeapon()) { return false; }
	AmmoState.bReloading = true;
	PublishAmmoState();
	return true;
}

bool UFE_EquipmentComponent::FinishWeaponReload()
{
	if (!GetOwner()->HasAuthority() || !AmmoState.bReloading || !CurrentWeaponData) { return false; }
	const FFE_WeaponAmmoSettings& Settings = CurrentWeaponData->Template.AmmoSettings;
	FFE_AmmoStack* Stack = AmmoInventory.FindByPredicate([&Settings](const FFE_AmmoStack& Entry) { return Entry.AmmoType == Settings.AmmoType; });
	const int32 Transfer = Stack ? FMath::Min(Stack->Count, FMath::Max(0, Settings.MagazineCapacity - CurrentWeaponInstance.LoadedAmmo)) : 0;
	if (Stack) { Stack->Count -= Transfer; }
	CurrentWeaponInstance.LoadedAmmo += Transfer;
	AmmoState.bReloading = false;
	SaveLoadedWeaponAmmo();
	PublishAmmoState();
	return Transfer > 0;
}

void UFE_EquipmentComponent::CancelWeaponReload()
{
	if (!GetOwner()->HasAuthority() || !AmmoState.bReloading) { return; }
	AmmoState.bReloading = false;
	PublishAmmoState();
}

void UFE_EquipmentComponent::CompleteAmmoAttack(const UFE_WeaponItemData* Weapon, bool bCharged, bool bShotConsumed)
{
	if (!GetOwner()->HasAuthority() || CurrentWeaponData != Weapon || !Weapon ||
		!Weapon->Template.AmmoSettings.bUsesAmmo || AmmoState.bReloading) { return; }
	if (bCharged)
	{
		// A cancelled charge does not spend a round and must not move reserve ammo either.
		if (bShotConsumed && BeginWeaponReload()) { FinishWeaponReload(); }
	}
	else if (CurrentWeaponInstance.LoadedAmmo == 0 && CanReloadWeapon()) { RequestReload(); }
}
