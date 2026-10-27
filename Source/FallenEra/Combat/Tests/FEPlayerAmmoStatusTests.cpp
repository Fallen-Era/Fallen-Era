#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/UI/FEPlayerStatusViewModel.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_PlayerAmmoStatusTest, "FallenEra.Combat.Ammo.StatusViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_PlayerAmmoStatusTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACharacter* Character = World->SpawnActor<ACharacter>();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Character);
	UFallenEraAbilitySystemComponent* ASC = NewObject<UFallenEraAbilitySystemComponent>(Character);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Character, Character);
	UFE_EquipmentComponent* Equipment = NewObject<UFE_EquipmentComponent>(Character);
	Equipment->RegisterComponent();
	UFE_RangedWeaponItemData* Weapon = NewObject<UFE_RangedWeaponItemData>(World);
	const FGameplayTag ItemTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	const FGameplayTag AmmoTag = FallenEraCombatGameplayTags::Item_Ammo_Rifle;
	Weapon->Template.ItemTags.AddTag(ItemTag);
	Weapon->Template.AmmoSettings.bUsesAmmo = true;
	Weapon->Template.AmmoSettings.AmmoType = AmmoTag;
	Weapon->Template.AmmoSettings.MagazineCapacity = 3;
	Weapon->Instance.LoadedAmmo = 3;
	*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("TestItemTags"))
		->ContainerPtrToValuePtr<TArray<FGameplayTag>>(Equipment) = { ItemTag };
	*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("WeaponDataAssets"))
		->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UFE_WeaponItemData>>>(Equipment) = { Weapon };
	Equipment->EquipWeaponByItemTag(ItemTag);
	Equipment->RefreshEquipment();
	Equipment->SetReserveAmmo(AmmoTag, 8);
	UFE_PlayerStatusViewModel* Model = NewObject<UFE_PlayerStatusViewModel>(Controller);
	Model->Initialize(Controller);
	TestTrue(TEXT("Initial equipped ammo weapon is visible"), Model->bUsesAmmo);
	TestEqual(TEXT("Initial magazine is read on binding"), Model->LoadedAmmo, 3);
	TestEqual(TEXT("Initial reserve excludes loaded rounds"), Model->ReserveAmmo, 8);

	int32 Notifications = 0;
	Model->AmmoChanged.AddLambda([&Notifications]() { ++Notifications; });
	Model->Initialize(Controller);
	Model->Initialize(Controller);
	Notifications = 0;
	Equipment->ConsumeWeaponAmmo(Weapon);
	TestEqual(TEXT("Shot updates magazine without polling"), Model->LoadedAmmo, 2);
	TestEqual(TEXT("Repeated Initialize does not duplicate subscriptions"), Notifications, 1);
	TestEqual(TEXT("Shot preserves reserve display"), Model->ReserveAmmo, 8);
	Equipment->BeginWeaponReload();
	Equipment->FinishWeaponReload();
	TestEqual(TEXT("Reload updates loaded display"), Model->LoadedAmmo, 3);
	TestEqual(TEXT("Reload updates reserve display"), Model->ReserveAmmo, 7);
	Equipment->SetReserveAmmo(AmmoTag, 0);
	TestEqual(TEXT("Empty reserve is displayed as zero"), Model->ReserveAmmo, 0);
	World->Tick(LEVELTICK_TimeOnly, 0.2f);
	Equipment->UnequipWeapon();
	TestFalse(TEXT("Unequip hides ammo"), Model->bUsesAmmo);
	TestEqual(TEXT("Unequip clears the previous weapon count"), Model->LoadedAmmo, 0);
	World->Tick(LEVELTICK_TimeOnly, 0.2f);
	Equipment->EquipWeaponByItemTag(ItemTag);
	Equipment->RefreshEquipment();
	TestTrue(TEXT("Re-equip shows ammo again"), Model->bUsesAmmo);
	Controller->UnPossess();
	TestFalse(TEXT("Unpossess hides the old pawn ammo"), Model->bUsesAmmo);
	Notifications = 0;
	Equipment->SetReserveAmmo(AmmoTag, 5);
	TestEqual(TEXT("Old pawn no longer sends updates"), Notifications, 0);
	Controller->Possess(Character);
	TestEqual(TEXT("Possession binds the new ammo source"), Model->ReserveAmmo, 5);
	Model->Shutdown();
	Notifications = 0;
	Equipment->SetReserveAmmo(AmmoTag, 6);
	TestEqual(TEXT("Shutdown removes equipment delegates"), Notifications, 0);
	Model->AmmoChanged.Clear();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
