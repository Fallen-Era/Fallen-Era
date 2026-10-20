#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Ability/FEHitscanAttackAbility.h"
#include "Combat/Ability/FEProjectileAttackAbility.h"
#include "Combat/Ability/FEBowAttackAbility.h"
#include "Combat/Ability/FEWeaponReloadAbility.h"
#include "Combat/Ability/FEWeaponAimAbility.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Projectile/FEArrowProjectile.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/Weapon/Attachment/FEWeaponSightData.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FAmmoFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ACharacter* Character;
		UFallenEraAbilitySystemComponent* ASC;
		UFE_EquipmentComponent* Equipment;
		UFE_RangedWeaponItemData* Weapon;
		FAmmoFixture(int32 Capacity = 3)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Character = World->SpawnActor<ACharacter>();
			ASC = NewObject<UFallenEraAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			ASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(Character));
			Equipment = NewObject<UFE_EquipmentComponent>(Character);
			Equipment->RegisterComponent();
			Weapon = NewObject<UFE_RangedWeaponItemData>(World);
			Weapon->Template.ItemTags.AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
			Weapon->Template.AmmoSettings.bUsesAmmo = true;
			Weapon->Template.AmmoSettings.AmmoType = FallenEraCombatGameplayTags::Item_Ammo_Rifle;
			Weapon->Template.AmmoSettings.MagazineCapacity = Capacity;
			UAnimMontage* ReloadMontage = NewObject<UAnimMontage>(Weapon);
			FindFProperty<FFloatProperty>(ReloadMontage->GetClass(), TEXT("SequenceLength"))
				->SetPropertyValue_InContainer(ReloadMontage, 0.1f);
			Weapon->Template.AmmoSettings.ReloadMontage = ReloadMontage;
			Weapon->Instance.LoadedAmmo = Capacity;
			*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("TestItemTags"))
				->ContainerPtrToValuePtr<TArray<FGameplayTag>>(Equipment) = { FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick };
			*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("WeaponDataAssets"))
				->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UFE_WeaponItemData>>>(Equipment) = { Weapon };
		}
		void Equip()
		{
			Equipment->EquipWeaponByItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
			Equipment->RefreshEquipment();
		}
		void Advance(float Seconds = 0.2f) { World->Tick(LEVELTICK_TimeOnly, Seconds); }
		~FAmmoFixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};

	class FReloadFlowCommand : public IAutomationLatentCommand
	{
	public:
		explicit FReloadFlowCommand(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Stage++ == 0)
			{
				Fixture = MakeUnique<FAmmoFixture>(1);
				UFE_HitscanAttackData* Attack = NewObject<UFE_HitscanAttackData>(Fixture->Weapon);
				Attack->InputTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
				Attack->AbilityClass = UFE_HitscanAttackAbility::StaticClass();
				Attack->bAutomatic = true;
				Attack->bDrawDebugTrace = false;
				Fixture->Weapon->Template.AttackActions.Add(Attack);
				Fixture->World->InitializeActorsForPlay(FURL());
				Fixture->World->BeginPlay();
				// Prime TimerManager before creating an ability task, as in an already-running game.
				Fixture->World->Tick(LEVELTICK_All, 0.01f);
				Fixture->Equip();
				Fixture->Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 2);
				Fixture->ASC->TryActivateAbilityByClass(UFE_HitscanAttackAbility::StaticClass());
				Test->TestEqual(TEXT("Last automatic shot consumes the magazine"), Fixture->Equipment->GetAmmoState().LoadedAmmo, 0);
				Test->TestTrue(TEXT("Last shot automatically starts the reload GA"), Fixture->Equipment->GetAmmoState().bReloading);
				Test->TestEqual(TEXT("Reserve is not spent before the timer completes"), Fixture->Equipment->GetAmmoState().ReserveAmmo, 2);
				return false;
			}
			if (Stage == 2)
			{
				Fixture->World->Tick(LEVELTICK_All, 0.2f);
				Test->TestFalse(TEXT("Reload timer ends the reload state"), Fixture->Equipment->GetAmmoState().bReloading);
				Test->TestEqual(TEXT("Reload timer fills the magazine"), Fixture->Equipment->GetAmmoState().LoadedAmmo, 1);
				Test->TestEqual(TEXT("Reload timer moves exactly one reserve round"), Fixture->Equipment->GetAmmoState().ReserveAmmo, 1);
				Fixture->Equipment->ConsumeWeaponAmmo(Fixture->Weapon);
				Test->TestTrue(TEXT("Manual reload activates the same GA"), Fixture->Equipment->RequestReload());
				FGameplayAbilitySpec* Reload = Fixture->ASC->FindAbilitySpecFromClass(UFE_WeaponReloadAbility::StaticClass());
				Fixture->ASC->CancelAbilityHandle(Reload->Handle);
				Test->TestFalse(TEXT("Cancelled reload clears the state"), Fixture->Equipment->GetAmmoState().bReloading);
				Test->TestEqual(TEXT("Cancelled reload keeps loose ammo"), Fixture->Equipment->GetAmmoState().ReserveAmmo, 1);
				Test->TestTrue(TEXT("Reload can restart immediately after cancellation"), Fixture->Equipment->RequestReload());
				Fixture->Equipment->UnequipWeapon();
				return false;
			}
			Fixture->World->Tick(LEVELTICK_All, 0.3f);
			Test->TestNull(TEXT("Weapon stays unequipped"), Fixture->Equipment->GetCurrentWeaponData());
			Test->TestFalse(TEXT("Swap clears reload state"), Fixture->Equipment->GetAmmoState().bReloading);
			Test->TestFalse(TEXT("Swap removes the GAS reload tag"), Fixture->ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading));
			Test->TestEqual(TEXT("Old reload timer cannot spend ammo after unequip"), Fixture->Equipment->GetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle), 1);
			return true;
		}
	private:
		FAutomationTestBase* Test;
		TUniquePtr<FAmmoFixture> Fixture;
		int32 Stage = 0;
	};

	class FReloadBlocksAimCommand : public IAutomationLatentCommand
	{
	public:
		explicit FReloadBlocksAimCommand(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Stage++ == 0)
			{
				Fixture = MakeUnique<FAmmoFixture>();
				Fixture->Character->AutoPossessAI = EAutoPossessAI::Disabled;
				Fixture->Weapon->Template.bSupportsAiming = true;
				Fixture->Weapon->Template.DefaultSight = NewObject<UFE_WeaponSightData>(Fixture->Weapon);
				// Data-only montage fixture: independent of renamed/moved project assets.
				UAnimMontage* Montage = NewObject<UAnimMontage>(Fixture->Weapon);
				FindFProperty<FFloatProperty>(Montage->GetClass(), TEXT("SequenceLength"))->SetPropertyValue_InContainer(Montage, 1.0f);
				Montage->RateScale = 0.5f;
				MontageDuration = Montage->GetPlayLength() / FMath::Abs(Montage->RateScale);
				Fixture->Weapon->Template.AmmoSettings.WeaponReloadMontage = Montage;
				Aiming = NewObject<UFE_WeaponAimingComponent>(Fixture->Character);
				Aiming->RegisterComponent();
				Fixture->World->InitializeActorsForPlay(FURL());
				Fixture->World->BeginPlay();
				Fixture->World->Tick(LEVELTICK_All, 0.01f);
				Fixture->Equip();
				Test->TestTrue(TEXT("Authority fixture has its sight ready"), Aiming->CanAim());
				Fixture->Equipment->ConsumeWeaponAmmo(Fixture->Weapon);
				Fixture->Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 3);
				Fixture->ASC->AbilityInputTagPressed(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick);
				Fixture->ASC->ProcessAbilityInput(0.0f, false);
				Test->TestTrue(TEXT("ADS starts before reload"), Aiming->IsAiming());
				Test->TestTrue(TEXT("Reload starts while aiming"), Fixture->Equipment->RequestReload());
				ReloadStartTime = Fixture->World->GetTimeSeconds();
				Test->TestFalse(TEXT("Reload cancels existing ADS"), Aiming->IsAiming());
				Test->TestFalse(TEXT("Component refuses direct ADS during reload"), Aiming->BeginAim());
				Test->TestFalse(TEXT("GAS refuses ADS activation during reload"), Fixture->ASC->TryActivateAbilityByClass(UFE_WeaponAimAbility::StaticClass()));
				Fixture->ASC->ProcessAbilityInput(0.0f, false);
				Test->TestFalse(TEXT("Held right-click cannot restart ADS during reload"), Aiming->IsAiming());
				return false;
			}
			if (Stage == 2)
			{
				Fixture->World->Tick(LEVELTICK_All, 0.2f);
				Fixture->ASC->ProcessAbilityInput(0.0f, false);
				Test->TestTrue(TEXT("Longer weapon montage keeps reload active after character montage ends"), Fixture->Equipment->GetAmmoState().bReloading);
				Test->TestFalse(TEXT("ADS stays blocked through the montage tail"), Aiming->IsAiming());
				return false;
			}
			// World clamps large delta times. Advance on separate engine frames instead.
			Fixture->World->Tick(LEVELTICK_All, 0.1f);
			if (Fixture->Equipment->GetAmmoState().bReloading && Stage < 40) { return false; }
			Test->TestFalse(TEXT("Reload ends after the montage duration"), Fixture->Equipment->GetAmmoState().bReloading);
			Test->TestTrue(TEXT("Slow montage RateScale extends the ADS lock"),
				Fixture->World->GetTimeSeconds() - ReloadStartTime >= MontageDuration - 0.01f);
			Fixture->ASC->ProcessAbilityInput(0.0f, false);
			Test->TestTrue(TEXT("Held right-click resumes ADS after reload finishes"), Aiming->IsAiming());
			FGameplayTagContainer AimTags(FallenEraCombatGameplayTags::Ability_Combat_Aim);
			Fixture->ASC->CancelAbilities(&AimTags);
			Fixture->ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading, 1, EGameplayTagReplicationState::TagAndCountToAll);
			Test->TestFalse(TEXT("Reload tag alone blocks component ADS"), Aiming->CanAim());
			Fixture->ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading, 1, EGameplayTagReplicationState::TagAndCountToAll);
			Test->TestTrue(TEXT("ADS becomes available when both reload states clear"), Aiming->CanAim());
			Test->TestTrue(TEXT("New reload starts for cancellation test"), Fixture->Equipment->ConsumeWeaponAmmo(Fixture->Weapon) && Fixture->Equipment->RequestReload());
			const FGameplayAbilitySpec* Reload = Fixture->ASC->FindAbilitySpecFromClass(UFE_WeaponReloadAbility::StaticClass());
			if (Test->TestNotNull(TEXT("Reload spec"), Reload)) { Fixture->ASC->CancelAbilityHandle(Reload->Handle); }
			Test->TestTrue(TEXT("Cancelled reload releases the ADS gate"), Aiming->CanAim());
			Test->TestTrue(TEXT("Ammo-only reload state starts"), Fixture->Equipment->BeginWeaponReload());
			Test->TestFalse(TEXT("Ammo-only state has no GAS reload tag yet"), Fixture->ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading));
			Test->TestFalse(TEXT("Ammo state alone blocks ADS before tag replication"), Aiming->CanAim());
			Fixture->Equipment->CancelWeaponReload();
			Test->TestTrue(TEXT("Ammo-only cancellation restores ADS"), Aiming->CanAim());
			return true;
		}
	private:
		FAutomationTestBase* Test;
		TUniquePtr<FAmmoFixture> Fixture;
		UFE_WeaponAimingComponent* Aiming = nullptr;
		float MontageDuration = 0.0f;
		float ReloadStartTime = 0.0f;
		int32 Stage = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ReloadBlocksAimTest, "FallenEra.Combat.Ammo.ReloadBlocksAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_ReloadBlocksAimTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FReloadBlocksAimCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoReloadGATest, "FallenEra.Combat.Ammo.ReloadAbilityLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoReloadGATest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FReloadFlowCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoNoMontageReloadTest, "FallenEra.Combat.Ammo.ReloadWithoutMontage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoNoMontageReloadTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	F.Weapon->Template.AmmoSettings.ReloadMontage.Reset();
	F.Weapon->Template.AmmoSettings.WeaponReloadMontage.Reset();
	F.Equip();
	F.Equipment->ConsumeWeaponAmmo(F.Weapon);
	F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 1);
	TestTrue(TEXT("Montage-free reload can activate"), F.Equipment->RequestReload());
	TestEqual(TEXT("No animation means immediate ammo transfer"), F.Equipment->GetAmmoState().LoadedAmmo, 3);
	TestEqual(TEXT("Immediate reload transfers reserve exactly once"), F.Equipment->GetAmmoState().ReserveAmmo, 0);
	TestFalse(TEXT("No hidden fallback reload duration"), F.Equipment->GetAmmoState().bReloading);
	TestFalse(TEXT("Immediate reload leaves no blocking tag"), F.ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoInvalidReloadRateTest, "FallenEra.Combat.Ammo.InvalidReloadPlayRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoInvalidReloadRateTest::RunTest(const FString& Parameters)
{
	for (const float Rate : {0.0f, -1.0f})
	{
		FAmmoFixture F;
		F.Weapon->Template.AmmoSettings.ReloadMontage.Get()->RateScale = Rate;
		F.Equip();
		F.Equipment->ConsumeWeaponAmmo(F.Weapon);
		F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 1);
		F.Equipment->RequestReload();
		TestEqual(TEXT("Invalid animation speed cannot instantly fill the magazine"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
		TestEqual(TEXT("Invalid animation speed preserves reserve ammo"), F.Equipment->GetAmmoState().ReserveAmmo, 1);
		TestFalse(TEXT("Invalid animation speed cannot leave reload stuck"), F.Equipment->GetAmmoState().bReloading);
		TestFalse(TEXT("Failed reload clears its ADS block tag"), F.ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoInventoryTest, "FallenEra.Combat.Ammo.InventoryAndReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoInventoryTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F(5);
	F.Equip();
	F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Arrow, 50);
	F.Equipment->ConsumeWeaponAmmo(F.Weapon);
	F.Equipment->ConsumeWeaponAmmo(F.Weapon);
	TestFalse(TEXT("Wrong ammo cannot reload a rifle"), F.Equipment->CanReloadWeapon());
	TestEqual(TEXT("DA seed remains unchanged"), F.Weapon->Instance.LoadedAmmo, 5);
	F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 1);
	TestTrue(TEXT("Begin partial reload"), F.Equipment->BeginWeaponReload());
	TestFalse(TEXT("No shots while reloading"), F.Equipment->CanFireWeaponAmmo(F.Weapon));
	TestEqual(TEXT("Starting reload does not remove reserve"), F.Equipment->GetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle), 1);
	F.Equipment->CancelWeaponReload();
	TestEqual(TEXT("Cancellation preserves magazine"), F.Equipment->GetAmmoState().LoadedAmmo, 3);
	TestTrue(TEXT("Restart reload"), F.Equipment->BeginWeaponReload());
	TestTrue(TEXT("Only available reserve transfers"), F.Equipment->FinishWeaponReload());
	TestEqual(TEXT("Partial magazine"), F.Equipment->GetAmmoState().LoadedAmmo, 4);
	TestEqual(TEXT("Reserve depleted"), F.Equipment->GetAmmoState().ReserveAmmo, 0);
	TestEqual(TEXT("Other ammo untouched"), F.Equipment->GetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Arrow), 50);
	TestFalse(TEXT("Completion cannot duplicate ammo"), F.Equipment->FinishWeaponReload());
	F.Advance();
	F.Equipment->UnequipWeapon();
	F.Advance();
	F.Equipment->EquipWeaponByItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
	TestEqual(TEXT("Unequip/re-equip does not refill from DA"), F.Equipment->GetAmmoState().LoadedAmmo, 4);
	F.Equipment->RefreshEquipment();
	TestEqual(TEXT("Refresh does not refill from DA"), F.Equipment->GetAmmoState().LoadedAmmo, 4);
	F.Character->SwapRoles();
	TestFalse(TEXT("Clients cannot edit reserve"), F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 999));
	TestFalse(TEXT("Clients cannot consume authoritative ammo"), F.Equipment->ConsumeWeaponAmmo(F.Weapon));
	TestFalse(TEXT("Clients cannot transfer reserve"), F.Equipment->BeginWeaponReload());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoReloadHeldInputTest, "FallenEra.Combat.Ammo.ReloadHeldInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoReloadHeldInputTest::RunTest(const FString& Parameters)
{
	for (bool bAutomatic : { false, true })
	{
		FAmmoFixture F(3);
		UFE_HitscanAttackData* Attack = NewObject<UFE_HitscanAttackData>(F.Weapon);
		Attack->InputTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
		Attack->AbilityClass = UFE_HitscanAttackAbility::StaticClass();
		Attack->bAutomatic = bAutomatic;
		Attack->bDrawDebugTrace = false;
		F.Weapon->Template.AttackActions.Add(Attack);
		F.Equip();
		const FGameplayTag Input = Attack->InputTag;
		const FGameplayTag Reload = FallenEraCombatGameplayTags::State_Combat_Reloading;
		F.ASC->AddLooseGameplayTag(Reload);
		F.ASC->AbilityInputTagPressed(Input);
		F.ASC->ProcessAbilityInput(0.016f, false);
		TestEqual(TEXT("Reload blocks the pressed shot"), F.Equipment->GetAmmoState().LoadedAmmo, 3);
		F.ASC->RemoveLooseGameplayTag(Reload);
		F.ASC->ProcessAbilityInput(0.016f, false);
		TestEqual(TEXT("Held input fires immediately when reload unblocks"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
		F.ASC->ProcessAbilityInput(0.016f, false);
		TestEqual(TEXT("One buffered press cannot fire twice in the same interval"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
		F.ASC->AbilityInputTagReleased(Input);
		F.ASC->ProcessAbilityInput(0.016f, false);
		F.Advance();
		F.ASC->AddLooseGameplayTag(Reload);
		F.ASC->AbilityInputTagPressed(Input);
		F.ASC->ProcessAbilityInput(0.016f, false);
		F.ASC->AbilityInputTagReleased(Input);
		F.ASC->ProcessAbilityInput(0.016f, false);
		F.ASC->RemoveLooseGameplayTag(Reload);
		F.ASC->ProcessAbilityInput(0.016f, false);
		TestEqual(TEXT("Releasing during reload discards the buffered shot"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
		F.ASC->AddLooseGameplayTag(Reload);
		F.ASC->AbilityInputTagPressed(Input);
		F.ASC->ProcessAbilityInput(0.016f, false);
		F.Equipment->ConsumeWeaponAmmo(F.Weapon);
		F.Equipment->ConsumeWeaponAmmo(F.Weapon);
		F.ASC->RemoveLooseGameplayTag(Reload);
		F.ASC->ProcessAbilityInput(0.016f, false);
		TestEqual(TEXT("Tag arriving before loaded ammo cannot fire"), F.Equipment->GetAmmoState().LoadedAmmo, 0);
		F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 3);
		F.Equipment->BeginWeaponReload();
		F.Equipment->FinishWeaponReload();
		F.ASC->ProcessAbilityInput(0.016f, false);
		TestEqual(TEXT("Buffered shot survives separated tag and ammo updates"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
		F.ASC->AbilityInputTagReleased(Input);
		F.ASC->ProcessAbilityInput(0.016f, false);
		F.ASC->ClearAbilityInput();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ProjectileReloadHeldInputTest, "FallenEra.Combat.Ammo.ProjectileReloadHeldInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_ProjectileReloadHeldInputTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F(3);
	UFE_ProjectileAttackData* Attack = NewObject<UFE_ProjectileAttackData>(F.Weapon);
	Attack->InputTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	Attack->AbilityClass = UFE_ProjectileAttackAbility::StaticClass();
	Attack->ProjectileClass = AFE_ArrowProjectile::StaticClass();
	Attack->bDrawDebugAim = false;
	F.Weapon->Template.AttackActions.Add(Attack);
	F.Equip();
	const FGameplayTag Reload = FallenEraCombatGameplayTags::State_Combat_Reloading;
	F.ASC->AddLooseGameplayTag(Reload);
	F.ASC->AbilityInputTagPressed(Attack->InputTag);
	F.ASC->ProcessAbilityInput(0.016f, false);
	F.ASC->RemoveLooseGameplayTag(Reload);
	F.ASC->ProcessAbilityInput(0.016f, false);
	TestEqual(TEXT("Immediate projectile fires on held input after reload"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
	F.Advance();
	F.ASC->ProcessAbilityInput(0.016f, false);
	TestEqual(TEXT("One-shot projectile does not become automatic"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
	F.ASC->ClearAbilityInput();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoHitscanTest, "FallenEra.Combat.Ammo.HitscanPerShot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoHitscanTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	UFE_HitscanAttackData* Attack = NewObject<UFE_HitscanAttackData>(F.Weapon);
	Attack->InputTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	Attack->AbilityClass = UFE_HitscanAttackAbility::StaticClass();
	Attack->bAutomatic = false;
	Attack->bDrawDebugTrace = false;
	Attack->PelletCount = 8;
	Attack->PenetrationCount = 3;
	F.Weapon->Template.AttackActions.Add(Attack);
	F.Equip();
	F.ASC->TryActivateAbilityByClass(UFE_HitscanAttackAbility::StaticClass());
	TestEqual(TEXT("Eight pellets consume exactly one cartridge"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
	F.ASC->TryActivateAbilityByClass(UFE_HitscanAttackAbility::StaticClass());
	TestEqual(TEXT("Interval rejection consumes nothing"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
	F.Advance();
	F.ASC->TryActivateAbilityByClass(UFE_HitscanAttackAbility::StaticClass());
	F.Advance();
	F.ASC->TryActivateAbilityByClass(UFE_HitscanAttackAbility::StaticClass());
	TestEqual(TEXT("Three shots empty the magazine"), F.Equipment->GetAmmoState().LoadedAmmo, 0);
	F.Advance();
	TestFalse(TEXT("Empty weapon cannot activate"), F.ASC->TryActivateAbilityByClass(UFE_HitscanAttackAbility::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoProjectileTest, "FallenEra.Combat.Ammo.ProjectilePerShot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoProjectileTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	UFE_ProjectileAttackData* Attack = NewObject<UFE_ProjectileAttackData>(F.Weapon);
	Attack->InputTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	Attack->AbilityClass = UFE_ProjectileAttackAbility::StaticClass();
	Attack->bDrawDebugAim = false;
	F.Weapon->Template.AttackActions.Add(Attack);
	F.Equip();
	F.ASC->TryActivateAbilityByClass(UFE_ProjectileAttackAbility::StaticClass());
	TestEqual(TEXT("Missing projectile class does not consume ammo"), F.Equipment->GetAmmoState().LoadedAmmo, 3);
	Attack->ProjectileClass = AFE_ArrowProjectile::StaticClass();
	F.ASC->TryActivateAbilityByClass(UFE_ProjectileAttackAbility::StaticClass());
	TestEqual(TEXT("Successful projectile consumes one round"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoChargeTest, "FallenEra.Combat.Ammo.ChargedReleaseAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoChargeTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F(1);
	F.Weapon->Template.AmmoSettings.AmmoType = FallenEraCombatGameplayTags::Item_Ammo_Arrow;
	UFE_BowAttackData* Attack = NewObject<UFE_BowAttackData>(F.Weapon);
	Attack->InputTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	Attack->AbilityClass = UFE_BowAttackAbility::StaticClass();
	Attack->ProjectileClass = AFE_ArrowProjectile::StaticClass();
	Attack->bDrawDebugAim = false;
	F.Weapon->Template.AttackActions.Add(Attack);
	F.Equip();
	F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Arrow, 2);
	F.ASC->TryActivateAbilityByClass(UFE_BowAttackAbility::StaticClass());
	TestEqual(TEXT("Starting charge keeps the loaded arrow"), F.Equipment->GetAmmoState().LoadedAmmo, 1);
	FGameplayAbilitySpec* Spec = F.ASC->FindAbilitySpecFromClass(UFE_BowAttackAbility::StaticClass());
	if (!TestNotNull(TEXT("Granted bow spec"), Spec)) { return false; }
	F.ASC->CancelAbilityHandle(Spec->Handle);
	TestEqual(TEXT("Cancellation does not use reserve"), F.Equipment->GetAmmoState().ReserveAmmo, 2);
	TestEqual(TEXT("Cancellation preserves the loaded arrow"), F.Equipment->GetAmmoState().LoadedAmmo, 1);
	F.ASC->TryActivateAbilityByClass(UFE_BowAttackAbility::StaticClass());
	F.ASC->AbilitySpecInputReleased(*Spec);
	TestFalse(TEXT("Release ends charged ability"), Spec->IsActive());
	TestEqual(TEXT("Release consumes one reserve arrow to prepare the next shot"), F.Equipment->GetAmmoState().ReserveAmmo, 1);
	TestEqual(TEXT("Next arrow is loaded without reload motion"), F.Equipment->GetAmmoState().LoadedAmmo, 1);
	TestFalse(TEXT("No delayed reload state for charged weapons"), F.Equipment->GetAmmoState().bReloading);
	return true;
}

#endif
