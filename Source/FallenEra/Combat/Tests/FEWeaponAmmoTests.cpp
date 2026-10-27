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
#include "Combat/Item/FEAmmoItemData.h"
#include "Combat/UI/FEAmmoSelectionViewModel.h"
#include "Combat/UI/FEAmmoSelectionInput.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/GameplayEffect/FEDamageExecutionCalculation.h"
#include "Combat/Tests/FEMeleeDamageTestActor.h"
#include "Combat/Ability/FEAmmoSelectionAbility.h"
#include "GameFramework/PlayerController.h"
#include "Combat/ObjectPool/FEProjectilePoolSubsystem.h"
#include "Components/ShapeComponent.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoInstanceExchangeTest, "FallenEra.Combat.Ammo.InstanceExchange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoInstanceExchangeTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	F.Weapon->Template.AmmoSettings.ReloadMontage.Reset();
	F.Equip();
	F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 8);
	FFE_AmmoStack Enhanced;
	Enhanced.AmmoType = FallenEraCombatGameplayTags::Item_Ammo_Rifle;
	Enhanced.Count = 6;
	Enhanced.Instance.ItemData.DamageMultiplier = 1.5f;
	TestTrue(TEXT("Generated enhanced ammo can enter inventory"), F.Equipment->AddAmmoInstance(Enhanced));
	const TArray<FFE_AmmoStack> Options = F.Equipment->GetCompatibleAmmo();
	TestEqual(TEXT("Same caliber with different stats never merges"), Options.Num(), 2);
	const FFE_AmmoStack* Strong = Options.FindByPredicate([](const auto& Stack) { return Stack.Instance.ItemData.DamageMultiplier == 1.5f; });
	const FFE_AmmoStack* Plain = Options.FindByPredicate([](const auto& Stack) { return Stack.Instance.ItemData.DamageMultiplier == 1.0f; });
	if (!Strong || !Plain) { AddError(TEXT("Missing ammo option")); return false; }
	const FGuid WeaponId = F.Equipment->GetAmmoState().WeaponInstanceId;
	TestFalse(TEXT("Forged stack identity cannot reload"), F.Equipment->RequestReloadAmmo(FGuid::NewGuid(), WeaponId));
	TestFalse(TEXT("Stale weapon identity cannot reload"), F.Equipment->RequestReloadAmmo(Strong->Instance.InstanceId, FGuid::NewGuid()));
	TestTrue(TEXT("Full magazine may exchange for a different variant"), F.Equipment->RequestReloadAmmo(Strong->Instance.InstanceId, WeaponId));
	TestEqual(TEXT("Exactly one variant is loaded"), F.Equipment->GetAmmoState().LoadedAmmoData.Instance.ItemData.DamageMultiplier, 1.5f);
	TestEqual(TEXT("Selected reserve excludes loaded rounds"), F.Equipment->GetAmmoState().ReserveAmmo, 3);
	TestEqual(TEXT("Exchange conserves total rounds"), F.Equipment->GetReserveAmmo(Enhanced.AmmoType) + F.Equipment->GetAmmoState().LoadedAmmo, 17);
	FFE_AmmoRoundData Shot;
	F.Equipment->ConsumeWeaponAmmo(F.Weapon, Shot);
	TestTrue(TEXT("Switching back works"), F.Equipment->RequestReloadAmmo(Plain->Instance.InstanceId, WeaponId));
	TestEqual(TEXT("Already-fired round retains its original stats"), Shot.Instance.ItemData.DamageMultiplier, 1.5f);
	TestEqual(TEXT("Exchange plus shot conserves rounds"), F.Equipment->GetReserveAmmo(Enhanced.AmmoType) + F.Equipment->GetAmmoState().LoadedAmmo, 16);
	F.Equipment->UnequipWeapon();
	F.Equip();
	TestEqual(TEXT("Re-equip preserves weapon identity"), F.Equipment->GetAmmoState().WeaponInstanceId, WeaponId);
	TestEqual(TEXT("Re-equip preserves chosen variant"), F.Equipment->GetAmmoState().SelectedAmmoId, Plain->Instance.InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoSelectionCancelTest, "FallenEra.Combat.Ammo.ExchangeCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoSelectionCancelTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	F.Equip();
	FFE_AmmoStack Enhanced;
	Enhanced.AmmoType = FallenEraCombatGameplayTags::Item_Ammo_Rifle;
	Enhanced.Count = 6;
	Enhanced.Instance.ItemData.DamageMultiplier = 1.2f;
	F.Equipment->AddAmmoInstance(Enhanced);
	TArray<FFE_AmmoStack> Options = F.Equipment->GetCompatibleAmmo();
	const FGuid StrongId = Options.Last().Instance.InstanceId;
	TestTrue(TEXT("Exchange can start with a full magazine"), F.Equipment->RequestReloadAmmo(StrongId, F.Equipment->GetAmmoState().WeaponInstanceId));
	TestEqual(TEXT("Starting reload does not unload old rounds"), F.Equipment->GetAmmoState().LoadedAmmo, 3);
	TestEqual(TEXT("Starting reload does not take reserve"), F.Equipment->GetReserveAmmo(Enhanced.AmmoType), 6);
	F.ASC->CancelAbilityHandle(F.ASC->FindAbilitySpecFromClass(UFE_WeaponReloadAbility::StaticClass())->Handle);
	TestEqual(TEXT("Cancel keeps old magazine stats"), F.Equipment->GetAmmoState().LoadedAmmoData.Instance.ItemData.DamageMultiplier, 1.0f);
	TestFalse(TEXT("Cancel clears reloading"), F.Equipment->GetAmmoState().bReloading);
	TestTrue(TEXT("Exchange can restart"), F.Equipment->RequestReloadAmmo(StrongId, F.Equipment->GetAmmoState().WeaponInstanceId));
	TArray<FFE_AmmoStack>& Runtime = *FindFProperty<FArrayProperty>(F.Equipment->GetClass(), TEXT("AmmoInventory"))->ContainerPtrToValuePtr<TArray<FFE_AmmoStack>>(F.Equipment);
	Runtime.FindByPredicate([StrongId](const auto& Stack) { return Stack.Instance.InstanceId == StrongId; })->Count = 0;
	TestFalse(TEXT("Inventory depletion is revalidated at completion"), F.Equipment->FinishWeaponReload());
	TestEqual(TEXT("Failed completion keeps the original loaded rounds"), F.Equipment->GetAmmoState().LoadedAmmo, 3);
	TestEqual(TEXT("Failed completion keeps the original selected variant"), F.Equipment->GetAmmoState().LoadedAmmoData.Instance.ItemData.DamageMultiplier, 1.0f);
	F.ASC->CancelAbilityHandle(F.ASC->FindAbilitySpecFromClass(UFE_WeaponReloadAbility::StaticClass())->Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoRadialModelTest, "FallenEra.Combat.Ammo.RadialSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoRadialModelTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Center cancels"), UFE_AmmoSelectionViewModel::FindSector(FVector2D::ZeroVector, 4, 80, 220), INDEX_NONE);
	TestEqual(TEXT("Outside cancels"), UFE_AmmoSelectionViewModel::FindSector(FVector2D(300, 0), 4, 80, 220), INDEX_NONE);
	TestEqual(TEXT("Upper right is first clockwise sector"), UFE_AmmoSelectionViewModel::FindSector(FVector2D(100, -100), 4, 80, 220), 0);
	TestEqual(TEXT("Lower right is second sector"), UFE_AmmoSelectionViewModel::FindSector(FVector2D(100, 100), 4, 80, 220), 1);
	TestEqual(TEXT("Upper left is last sector"), UFE_AmmoSelectionViewModel::FindSector(FVector2D(-100, -100), 4, 80, 220), 3);
	FAmmoFixture F;
	F.Equip();
	F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 2);
	UFE_AmmoSelectionViewModel* Model = NewObject<UFE_AmmoSelectionViewModel>();
	Model->Initialize(F.Equipment);
	TestEqual(TEXT("One compatible option"), Model->Options.Num(), 1);
	Model->SelectIndex(0);
	TestTrue(TEXT("Positive count can be selected"), Model->GetSelectedAmmoId().IsValid());
	F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 0);
	TestEqual(TEXT("Inventory delegate updates open menu without polling"), Model->Options[0].Count, 0);
	TestFalse(TEXT("Depleted selection is cleared"), Model->GetSelectedAmmoId().IsValid());
	Model->SelectIndex(0);
	TestFalse(TEXT("Zero-count option cannot be selected"), Model->GetSelectedAmmoId().IsValid());
	Model->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoInputTapTest, "FallenEra.Combat.Ammo.ReloadTapInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoInputTapTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	APlayerController* Controller = F.World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(F.Character);
	F.ASC->RefreshAbilityActorInfo();
	F.Weapon->Template.AmmoSettings.ReloadMontage.Reset();
	F.World->InitializeActorsForPlay(FURL());
	F.World->BeginPlay();
	F.World->Tick(LEVELTICK_All, 0.01f);
	F.Equip();
	F.Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 6);
	F.Equipment->ConsumeWeaponAmmo(F.Weapon);
	const FGameplayTag Input = FallenEraCombatGameplayTags::Ability_Input_Combat_Reload;
	F.ASC->AbilityInputTagPressed(Input);
	F.ASC->ProcessAbilityInput(0.01f, false);
	TestEqual(TEXT("R press waits instead of immediately reloading"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
	FGameplayAbilitySpec* Selection = F.ASC->FindAbilitySpecFromClass(UFE_AmmoSelectionAbility::StaticClass());
	TestTrue(TEXT("Local input GA is active while R is held"), Selection && Selection->IsActive());
	F.ASC->AbilityInputTagReleased(Input);
	F.ASC->ProcessAbilityInput(0.01f, false);
	TestEqual(TEXT("Short R release starts the server reload"), F.Equipment->GetAmmoState().LoadedAmmo, 3);
	TestFalse(TEXT("Release ends the local input GA"), Selection && Selection->IsActive());
	F.World->Tick(LEVELTICK_All, 0.2f); // Respect the equipment component's 0.1s swap throttle.
	F.Equipment->ConsumeWeaponAmmo(F.Weapon);
	F.ASC->AbilityInputTagPressed(Input);
	F.ASC->ProcessAbilityInput(0.01f, false);
	F.Equipment->UnequipWeapon();
	TestNull(TEXT("Fixture actually unequips before testing cancellation"), F.Equipment->GetCurrentWeaponData());
	F.ASC->AbilityInputTagReleased(Input);
	F.ASC->ProcessAbilityInput(0.01f, false);
	TestFalse(TEXT("Weapon change cancels any selection state"), F.ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_SelectingAmmo));
	TestEqual(TEXT("Cancelled hold cannot spend reserve"), F.Equipment->GetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoProjectileSnapshotTest, "FallenEra.Combat.Ammo.ProjectileSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoProjectileSnapshotTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	F.World->InitializeActorsForPlay(FURL());
	F.World->BeginPlay();
	F.Equip();
	UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(F.Character);
	Combat->RegisterComponent();
	F.ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 10.0f);
	AFE_MeleeDamageTestActor* Target = F.World->SpawnActor<AFE_MeleeDamageTestActor>(FVector(1000, 0, 0), FRotator::ZeroRotator);
	UFE_ProjectilePoolSubsystem* Pool = F.World->GetSubsystem<UFE_ProjectilePoolSubsystem>();
	if (!TestNotNull(TEXT("Projectile pool exists"), Pool)) { return false; }
	AFE_CombatProjectile* Projectile = Pool->AcquireProjectile(AFE_ArrowProjectile::StaticClass(), FTransform(FVector(100, 0, 0)), F.Character, F.Character, false);
	if (!TestNotNull(TEXT("Pooled arrow"), Projectile)) { return false; }
	Projectile->InitializeProjectile(F.Character, F.Weapon, nullptr, UFE_DamageGameplayEffect::StaticClass(), FVector(3000, 0, 0), 0.0f);
	Projectile->SetAmmoDamageMultiplier(1.5f);
	TestTrue(TEXT("Projectile hit delegate is registered"), Projectile->GetActiveCollisionComponent()->OnComponentHit.IsBound());
	TestTrue(TEXT("Projectile impact runs on authority"), Projectile->HasAuthority());
	const FHitResult Hit(Target, Cast<UPrimitiveComponent>(Target->GetRootComponent()), Target->GetActorLocation(), FVector(-1, 0, 0));
	Projectile->GetActiveCollisionComponent()->OnComponentHit.Broadcast(Projectile->GetActiveCollisionComponent(), Target,
		Cast<UPrimitiveComponent>(Target->GetRootComponent()), FVector::ZeroVector, Hit);
	TestEqual(TEXT("Non-ASC receiver gets effective attack power including ammo"), Target->LastRequest.SourceAttackPower, 15.0f);
	TestEqual(TEXT("Non-ASC receiver can use its existing defense formula"), Target->Health, 990.0f);
	AFE_CombatProjectile* Reused = Pool->AcquireProjectile(AFE_ArrowProjectile::StaticClass(), FTransform(FVector(100, 0, 0)), F.Character, F.Character, false);
	TestTrue(TEXT("Same actor is reused"), Reused == Projectile);
	Reused->InitializeProjectile(F.Character, F.Weapon, nullptr, UFE_DamageGameplayEffect::StaticClass(), FVector(3000, 0, 0), 0.0f);
	Reused->GetActiveCollisionComponent()->OnComponentHit.Broadcast(Reused->GetActiveCollisionComponent(), Target,
		Cast<UPrimitiveComponent>(Target->GetRootComponent()), FVector::ZeroVector, Hit);
	TestEqual(TEXT("Pool reuse cannot retain the previous enhanced multiplier"), Target->LastRequest.SourceAttackPower, 10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoConfiguredAssetsTest, "FallenEra.Combat.Ammo.ConfiguredDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoConfiguredAssetsTest::RunTest(const FString& Parameters)
{
	for (const FString Caliber : {TEXT("9mm"), TEXT("5mm"), TEXT("7mm")})
	{
		TestTrue(TEXT("Caliber is actually registered in the gameplay-tag dictionary"),
			FGameplayTag::RequestGameplayTag(FName(*(TEXT("Item.Ammo.") + Caliber)), false).IsValid());
		for (const FString Variant : {TEXT("Standard"), TEXT("Enhanced")})
		{
			const FString Name = TEXT("DA_Ammo_") + Caliber + TEXT("_") + Variant;
			const FString Path = TEXT("/Game/Combat/Equiment/Ammo/") + Name + TEXT(".") + Name;
			const UFE_AmmoItemData* Definition = LoadObject<UFE_AmmoItemData>(nullptr, *Path);
			if (!TestNotNull(*Name, Definition)) { continue; }
			TestEqual(TEXT("Definition exact caliber"), Definition->Template.AmmoType.ToString(), TEXT("Item.Ammo.") + Caliber);
			TestTrue(TEXT("Variant identity is registered for editor/fast replication"),
				FGameplayTag::RequestGameplayTag(Definition->Template.ItemTag.GetTagName(), false).IsValid());
			TestTrue(TEXT("DA seed stats are valid and remain designer-editable"), FMath::IsFinite(Definition->Instance.ItemData.DamageMultiplier) && Definition->Instance.ItemData.DamageMultiplier >= 0.0f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoDamageExecutionTest, "FallenEra.Combat.Ammo.DamageExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoDamageExecutionTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	ACharacter* Target = F.World->SpawnActor<ACharacter>(FVector(1000, 0, 0), FRotator::ZeroRotator);
	UAbilitySystemComponent* TargetASC = NewObject<UAbilitySystemComponent>(Target);
	TargetASC->RegisterComponent();
	TargetASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(Target));
	TargetASC->InitAbilityActorInfo(Target, Target);
	TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetDefensePowerAttribute(), 5.0f);
	F.ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 10.0f);
	UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(Target);
	Combat->RegisterComponent();
	TGuardValue<float> NoVariance(GetMutableDefault<UFE_DamageExecutionCalculation>()->DamageVarianceRatio, 0.0f);
	FFE_CombatDamageRequest Request;
	Request.SourceActor = F.Character;
	Request.TargetActor = Target;
	Request.DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
	for (const float Multiplier : {1.0f, 1.5f, 0.0f})
	{
		TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetHealthAttribute(), 100.0f);
		Request.AmmoDamageMultiplier = Multiplier;
		const auto Result = Combat->ApplyGameplayEffectDamage(Request);
		TestEqual(TEXT("Ammo multiplies attack BEFORE defense, exactly once"), Result.AppliedDamage, FMath::Max(0.0f, 10.0f * Multiplier - 5.0f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoSelectionInputRestoreTest, "FallenEra.Combat.Ammo.InputRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoSelectionInputRestoreTest::RunTest(const FString& Parameters)
{
	FAmmoFixture F;
	APlayerController* Controller = F.World->SpawnActor<APlayerController>();
	FFE_AmmoSelectionInput Input;
	Controller->bShowMouseCursor = false;
	Controller->SetIgnoreLookInput(true); // An existing modal owns a separate look lock.
	Input.Capture(Controller, FSimpleDelegate());
	Input.Capture(Controller, FSimpleDelegate()); // Duplicate open must not stack locks.
	TestTrue(TEXT("Menu captures input once"), Input.IsCaptured());
	TestTrue(TEXT("Menu shows the UI cursor"), Controller->bShowMouseCursor);
	Input.Restore();
	Input.Restore();
	TestFalse(TEXT("Closing never depends on wheel focus"), Input.IsCaptured());
	TestFalse(TEXT("Closing always restores the previous hidden cursor"), Controller->bShowMouseCursor);
	TestTrue(TEXT("Closing preserves another system's look lock"), Controller->IsLookInputIgnored());
	Controller->SetIgnoreLookInput(false);
	TestFalse(TEXT("Exactly one menu look lock was removed"), Controller->IsLookInputIgnored());
	for (int32 Cycle = 0; Cycle < 3; ++Cycle)
	{
		Input.Capture(Controller, FSimpleDelegate());
		Input.Restore();
	}
	TestFalse(TEXT("Repeated hold/release cycles cannot leave a cursor"), Controller->bShowMouseCursor);
	TestFalse(TEXT("Repeated hold/release cycles cannot leak look locks"), Controller->IsLookInputIgnored());
	Controller->bShowMouseCursor = true;
	Input.Capture(Controller, FSimpleDelegate());
	Input.Restore();
	TestTrue(TEXT("A cursor shown before opening remains shown"), Controller->bShowMouseCursor);
	return true;
}

#endif
