#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/Armor/FEArmorItemData.h"
#include "Combat/FECombatGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_EquipmentInstanceTest, "FallenEra.Combat.Items.InstanceIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_EquipmentInstanceTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	const FGameplayTag WeaponTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	UFE_MeleeWeaponItemData* Weapon = NewObject<UFE_MeleeWeaponItemData>(World);
	Weapon->Template.ItemTags.AddTag(WeaponTag);
	Weapon->Template.AttachSocketName = NAME_None;
	Weapon->Template.WeaponStat.Offense = 99.0f;
	Weapon->Instance.ItemData.Offense = 7.0f;
	Weapon->Instance.ItemData.Accuracy = 0.85f;
	Weapon->Instance.ItemData.RecoilControl = 0.6f;
	UFE_EquipmentComponent* Equipment[2];
	UAbilitySystemComponent* ASC[2];
	for (int32 Index = 0; Index < 2; ++Index)
	{
		ACharacter* Character = World->SpawnActor<ACharacter>(FVector(Index * 1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		ASC[Index] = NewObject<UAbilitySystemComponent>(Character);
		ASC[Index]->RegisterComponent();
		ASC[Index]->InitAbilityActorInfo(Character, Character);
		UFallenEraAttributeSet* Stats = NewObject<UFallenEraAttributeSet>(Character);
		ASC[Index]->AddAttributeSetSubobject(Stats);
		Stats->InitAttackPower(10.0f);
		Stats->InitDefensePower(2.0f);
		Equipment[Index] = NewObject<UFE_EquipmentComponent>(Character);
		Equipment[Index]->RegisterComponent();
		*FindFProperty<FArrayProperty>(Equipment[Index]->GetClass(), TEXT("TestItemTags"))
			->ContainerPtrToValuePtr<TArray<FGameplayTag>>(Equipment[Index]) = { WeaponTag };
		*FindFProperty<FArrayProperty>(Equipment[Index]->GetClass(), TEXT("WeaponDataAssets"))
			->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UFE_WeaponItemData>>>(Equipment[Index]) = { Weapon };
		Equipment[Index]->EquipWeaponByItemTag(WeaponTag);
		Equipment[Index]->RefreshEquipment();
		TestEqual(TEXT("DA test Instance, not Template baseline, supplies offense"),
			ASC[Index]->GetNumericAttribute(UFallenEraAttributeSet::GetAttackPowerAttribute()), 17.0f);
		TestEqual(TEXT("Accuracy still uses equipped baseline deviation"),
			ASC[Index]->GetNumericAttribute(UFallenEraAttributeSet::GetAccuracyAttribute()), 0.85f);
	}
	TestTrue(TEXT("Two copies of one DA have different identities"),
		Equipment[0]->GetWeaponInstanceData().InstanceId != Equipment[1]->GetWeaponInstanceData().InstanceId);
	FFE_WeaponInstanceData Generated = Weapon->Instance;
	Generated.ItemData.Offense = 30.0f;
	TestTrue(TEXT("Generated instance replaces the active weapon stats"), Equipment[0]->SetWeaponInstanceData(Generated));
	Equipment[0]->RefreshEquipment();
	TestEqual(TEXT("Refresh preserves generated instance without GE stacking"),
		ASC[0]->GetNumericAttribute(UFallenEraAttributeSet::GetAttackPowerAttribute()), 40.0f);
	TestEqual(TEXT("Other character's copy is unchanged"),
		ASC[1]->GetNumericAttribute(UFallenEraAttributeSet::GetAttackPowerAttribute()), 17.0f);
	TestEqual(TEXT("Shared DA test seed is unchanged"), Weapon->Instance.ItemData.Offense, 7.0f);
	Generated.ItemData.Accuracy = 2.0f;
	TestFalse(TEXT("Invalid generated stats are rejected"), Equipment[0]->SetWeaponInstanceData(Generated));
	TestEqual(TEXT("Rejected update preserves current stats"),
		ASC[0]->GetNumericAttribute(UFallenEraAttributeSet::GetAttackPowerAttribute()), 40.0f);
	UFE_ArmorItemData* Armor = NewObject<UFE_ArmorItemData>(World);
	const FGameplayTag Slot = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Body"));
	Armor->Template.EquipmentSlotTag = Slot;
	Armor->Template.ArmorStat.Defense = 99.0f;
	Armor->Instance.ItemData.Defense = 4.0f;
	Armor->Instance.ItemData.KnockbackResistance = 100.0f;
	TestTrue(TEXT("Default armor reads DA Instance"), Equipment[0]->EquipArmor(Armor));
	FFE_ArmorInstanceData GeneratedArmor = Armor->Instance;
	GeneratedArmor.ItemData.Defense = 9.0f;
	TestTrue(TEXT("Generated armor values are accepted"), Equipment[1]->EquipArmorInstance(Armor, GeneratedArmor));
	Equipment[1]->RefreshEquipment();
	TestEqual(TEXT("First armor copy keeps its own defense"),
		ASC[0]->GetNumericAttribute(UFallenEraAttributeSet::GetDefensePowerAttribute()), 6.0f);
	TestEqual(TEXT("Second armor copy keeps generated defense through refresh"),
		ASC[1]->GetNumericAttribute(UFallenEraAttributeSet::GetDefensePowerAttribute()), 11.0f);
	TestEqual(TEXT("Armor DA is never modified"), Armor->Instance.ItemData.Defense, 4.0f);
	TestTrue(TEXT("Armor removal removes only its own effect"), Equipment[1]->UnequipArmor(Slot));
	TestEqual(TEXT("Removing instance restores base defense"),
		ASC[1]->GetNumericAttribute(UFallenEraAttributeSet::GetDefensePowerAttribute()), 2.0f);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ItemTemplateMigrationTest, "FallenEra.Combat.Items.TemplateMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ItemTemplateMigrationTest::RunTest(const FString& Parameters)
{
	UFE_MeleeWeaponItemData* Weapon = NewObject<UFE_MeleeWeaponItemData>();
	FindFProperty<FStructProperty>(Weapon->GetClass(), TEXT("WeaponStat"))
		->ContainerPtrToValuePtr<FFE_WeaponStat>(Weapon)->Offense = 12.0f;
	FindFProperty<FStructProperty>(Weapon->GetClass(), TEXT("ItemTags"))
		->ContainerPtrToValuePtr<FGameplayTagContainer>(Weapon)->AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
	Weapon->PostLoad();
	TestEqual(TEXT("Legacy baseline moves into Template"), Weapon->Template.WeaponStat.Offense, 12.0f);
	TestEqual(TEXT("Legacy baseline seeds test Instance"), Weapon->Instance.ItemData.Offense, 12.0f);
	TestTrue(TEXT("Legacy identity tags preserved"), Weapon->HasItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick));
	Weapon->Instance.ItemData.Offense = 20.0f;
	Weapon->PostLoad();
	TestEqual(TEXT("Migration does not overwrite a new Instance"), Weapon->Instance.ItemData.Offense, 20.0f);
	TArray<uint8> SavedBytes;
	{
		FMemoryWriter Writer(SavedBytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Weapon->Serialize(Archive);
	}
	UFE_MeleeWeaponItemData* Reloaded = NewObject<UFE_MeleeWeaponItemData>();
	{
		FMemoryReader Reader(SavedBytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		Reloaded->Serialize(Archive);
	}
	Reloaded->PostLoad();
	TestEqual(TEXT("Saving and reloading new format preserves Template"), Reloaded->Template.WeaponStat.Offense, 12.0f);
	TestEqual(TEXT("Saving and reloading new format preserves edited Instance"), Reloaded->Instance.ItemData.Offense, 20.0f);
	for (const TCHAR* Path : {
		TEXT("/Game/Combat/Equiment/Pistol/DA_PistolData.DA_PistolData"),
		TEXT("/Game/Combat/Equiment/Rifle/DA_RifleData.DA_RifleData"),
		TEXT("/Game/Combat/Equiment/Bow/DA_Bow.DA_Bow"),
		TEXT("/Game/Combat/Equiment/Axe/DA_Weapon_AxeData.DA_Weapon_AxeData") })
	{
		UFE_WeaponItemData* Asset = LoadObject<UFE_WeaponItemData>(nullptr, Path);
		if (!TestNotNull(Path, Asset)) { continue; }
		TestTrue(TEXT("Existing weapon retains identity"), !Asset->Template.ItemTags.IsEmpty() || !Asset->Template.WeaponTags.IsEmpty());
		TestTrue(TEXT("Existing weapon retains attacks"), !Asset->Template.AttackActions.IsEmpty());
		TestTrue(TEXT("Existing weapon Instance is valid"), Asset->Instance.IsValid());
		// Existing, already-migrated items may intentionally override their template baseline.
		// Exact legacy seeding and persistence are checked by the synthetic migration/round-trip above.
		AddInfo(FString::Printf(TEXT("%s: Template offense=%g, Instance offense=%g"), Path,
			Asset->Template.WeaponStat.Offense, Asset->Instance.ResolveStats().Offense));
	}
	UFE_ArmorItemData* Armor = LoadObject<UFE_ArmorItemData>(nullptr, TEXT("/Game/Combat/Equiment/Armor/DA_ArmorData.DA_ArmorData"));
	if (TestNotNull(TEXT("Existing armor"), Armor))
	{
		TestTrue(TEXT("Existing armor keeps its slot"), Armor->Template.EquipmentSlotTag.IsValid());
		TestEqual(TEXT("Existing armor defense migrates intact"), Armor->Instance.ResolveStats().Defense, Armor->Template.ArmorStat.Defense);
	}
	return true;
}

#endif
