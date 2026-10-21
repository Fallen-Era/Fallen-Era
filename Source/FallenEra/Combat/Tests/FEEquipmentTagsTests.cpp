#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_EquipmentTagsTest, "FallenEra.Combat.Equipment.EquippedTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_EquipmentTagsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACharacter* Character = World->SpawnActor<ACharacter>();
	UFE_EquipmentComponent* Equipment = NewObject<UFE_EquipmentComponent>(Character);
	Character->AddInstanceComponent(Equipment);
	Equipment->RegisterComponent();
	FindFProperty<FFloatProperty>(Equipment->GetClass(), TEXT("EquipmentChangeInterval"))
		->SetPropertyValue_InContainer(Equipment, 0.0f);
	FindFProperty<FBoolProperty>(Equipment->GetClass(), TEXT("bPreloadWeaponAssetsOnBeginPlay"))
		->SetPropertyValue_InContainer(Equipment, false);
	const FGameplayTag HammerTag = FallenEraCombatGameplayTags::Equipment_Weapon_Hammer;
	const FGameplayTag OtherTag = FallenEraCombatGameplayTags::Equipment_WeaponSlot_Sight;
	UFE_MeleeWeaponItemData* Hammer = NewObject<UFE_MeleeWeaponItemData>(World);
	Hammer->Template.ItemTags.AddTag(HammerTag);
	Hammer->Template.EquippedTags.AddTag(HammerTag);
	UFE_MeleeWeaponItemData* Other = NewObject<UFE_MeleeWeaponItemData>(World);
	Other->Template.ItemTags.AddTag(OtherTag);
	Other->Template.WeaponTags.AddTag(HammerTag); // Classification alone must not claim that a hammer is equipped.
	*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("TestItemTags"))
		->ContainerPtrToValuePtr<TArray<FGameplayTag>>(Equipment) = {HammerTag, OtherTag};
	*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("WeaponDataAssets"))
		->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UFE_WeaponItemData>>>(Equipment) = {Hammer, Other};
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	// This lightweight test world has no GameMode to dispatch actor BeginPlay.
	if (!Character->HasActorBegunPlay()) { Character->DispatchBeginPlay(); }
	TestTrue(TEXT("Fixture runs the real equipment BeginPlay/EndPlay lifecycle"), Equipment->HasBegunPlay());
	Equipment->EquipWeaponByItemTag(HammerTag);
	TestTrue(TEXT("Equipment can load before the pawn ASC is available"), Equipment->GetCurrentWeaponData() == Hammer);

	auto AddASC = [Character]()
	{
		UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Character);
		ASC->RegisterComponent();
		ASC->InitAbilityActorInfo(Character, Character);
		ASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(ASC));
		return ASC;
	};
	UAbilitySystemComponent* ASC = AddASC();
	Equipment->RefreshEquipment();
	TestTrue(TEXT("Late ASC receives the equipped hammer tag on refresh"), ASC->HasMatchingGameplayTag(HammerTag));
	TestEqual(TEXT("One equipment contribution"), ASC->GetGameplayTagCount(HammerTag), 1);
	Equipment->RefreshEquipment();
	Equipment->RefreshEquipment();
	TestEqual(TEXT("Refresh does not stack equipment tag counts"), ASC->GetGameplayTagCount(HammerTag), 1);

	ASC->AddLooseGameplayTag(HammerTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	Equipment->UnequipWeapon();
	TestEqual(TEXT("Unequip preserves another source's tag count"), ASC->GetGameplayTagCount(HammerTag), 1);
	ASC->RemoveLooseGameplayTag(HammerTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	TestFalse(TEXT("No equipped hammer after external contribution is removed"), ASC->HasMatchingGameplayTag(HammerTag));
	Equipment->EquipWeaponByItemTag(HammerTag);
	Equipment->EquipWeaponByItemTag(OtherTag);
	TestFalse(TEXT("Swapping to another weapon clears hammer status, even with matching classification tags"), ASC->HasMatchingGameplayTag(HammerTag));
	Equipment->EquipWeaponByItemTag(HammerTag);

	// Keep the old ASC alive to simulate a PlayerState/actor-info replacement.
	Character->RemoveOwnedComponent(ASC);
	UAbilitySystemComponent* ReplacementASC = AddASC();
	Equipment->RefreshEquipment();
	TestFalse(TEXT("ASC handoff clears the old ASC, not the new one"), ASC->HasMatchingGameplayTag(HammerTag));
	TestEqual(TEXT("Replacement ASC gets exactly one contribution"), ReplacementASC->GetGameplayTagCount(HammerTag), 1);
	Equipment->DestroyComponent();
	TestFalse(TEXT("Equipment EndPlay cleans up tags on a surviving ASC"), ReplacementASC->HasMatchingGameplayTag(HammerTag));
	ASC->DestroyComponent();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
