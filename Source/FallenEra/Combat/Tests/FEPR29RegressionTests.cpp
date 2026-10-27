#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/Ability/FEMeleeAttackAbility.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEStaminaComponent.h"
#include "Combat/Component/FEPlayerMovementStatsComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Tests/FEEnemySettingsTestCharacter.h"
#include "Combat/Tests/FEEnemyObstacleTestActor.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FFE_PR29Fixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ACharacter* Character = nullptr;
		UFallenEraAbilitySystemComponent* ASC = nullptr;
		UFE_EquipmentComponent* Equipment = nullptr;
		UFE_RangedWeaponItemData* Weapon = nullptr;
		FFE_PR29Fixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Character = World->SpawnActor<ACharacter>();
			ASC = NewObject<UFallenEraAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(Character));
			ASC->InitAbilityActorInfo(Character, Character);
			Equipment = NewObject<UFE_EquipmentComponent>(Character);
			Equipment->RegisterComponent();
			FindFProperty<FFloatProperty>(Equipment->GetClass(), TEXT("EquipmentChangeInterval"))->SetPropertyValue_InContainer(Equipment, 0.0f);
			Weapon = NewObject<UFE_RangedWeaponItemData>(World);
			Weapon->Template.ItemTags.AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
			Weapon->Template.bSupportsAiming = true;
			Weapon->Template.AttachSocketName = NAME_None;
			Weapon->Instance.ItemData.Offense = 7.0f;
			Weapon->Instance.ItemData.Accuracy = 0.5f;
			Weapon->Template.AmmoSettings.bUsesAmmo = true;
			Weapon->Template.AmmoSettings.AmmoType = FallenEraCombatGameplayTags::Item_Ammo_Rifle;
			Weapon->Template.AmmoSettings.MagazineCapacity = 3;
			Weapon->Instance.LoadedAmmo = 3;
			*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("TestItemTags"))->ContainerPtrToValuePtr<TArray<FGameplayTag>>(Equipment) =
				{ FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick, FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick };
			UFE_RangedWeaponItemData* Other = NewObject<UFE_RangedWeaponItemData>(World);
			Other->Template.ItemTags.AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick);
			Other->Template.AttachSocketName = NAME_None;
			*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("WeaponDataAssets"))->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UFE_WeaponItemData>>>(Equipment) = { Weapon, Other };
			Equipment->EquipWeaponByItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
		}
		~FFE_PR29Fixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_PR29RestoreTest, "FallenEra.Combat.PR29.WeaponRestoreBeforeDerivedState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_PR29RestoreTest::RunTest(const FString& Parameters)
{
	FFE_PR29Fixture F;
	UFE_WeaponAttachmentData* Part = NewObject<UFE_WeaponAttachmentData>(F.World);
	Part->SlotTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	Part->AccuracyBonus = 0.1f;
	FFE_WeaponInstanceData Generated = F.Equipment->GetWeaponInstanceData();
	Generated.ItemData.Offense = 30.0f;
	Generated.StaminaCost = 2.0f;
	Generated.bOverrideAttachments = true;
	Generated.Attachments.AddDefaulted_GetRef().Definition = Part;
	TestTrue(TEXT("Generated item accepted"), F.Equipment->SetWeaponInstanceData(Generated));
	TestTrue(TEXT("Shot consumes a round"), F.Equipment->ConsumeWeaponAmmo(F.Weapon));
	F.Equipment->EquipWeaponByItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick);
	F.Equipment->EquipWeaponByItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
	TestEqual(TEXT("Restored instance drives GAS offense before applying the GE"), F.ASC->GetNumericAttribute(UFallenEraAttributeSet::GetAttackPowerAttribute()), 30.0f);
	TestEqual(TEXT("Restored part bonus drives GAS accuracy"), F.ASC->GetNumericAttribute(UFallenEraAttributeSet::GetAccuracyAttribute()), 0.6f);
	TestEqual(TEXT("Instance identity survives swapping"), F.Equipment->GetWeaponInstanceId(), Generated.InstanceId);
	TestEqual(TEXT("No per-shot full snapshot is needed to retain magazine state"), F.Equipment->GetAmmoState().LoadedAmmo, 2);
	TestEqual(TEXT("Owner snapshot contains the generated stamina cost"), F.Equipment->GetAmmoState().StaminaCost, 2.0f);
	TestEqual(TEXT("Custom attachment is restored"), F.Equipment->GetWeaponAttachments().Num(), 1);
	TestEqual(TEXT("Shared asset remains unchanged"), F.Weapon->Instance.ItemData.Offense, 7.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_PR29PredictionTest, "FallenEra.Combat.PR29.OwnerCostPrediction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_PR29PredictionTest::RunTest(const FString& Parameters)
{
	FFE_PR29Fixture F;
	FFE_WeaponInstanceData Generated = F.Equipment->GetWeaponInstanceData();
	Generated.StaminaCost = 2.0f;
	F.Equipment->SetWeaponInstanceData(Generated);
	// Recreate the receiving client's transient DA seed while retaining the received owner-only snapshot.
	FindFProperty<FStructProperty>(F.Equipment->GetClass(), TEXT("CurrentWeaponInstance"))->ContainerPtrToValuePtr<FFE_WeaponInstanceData>(F.Equipment)->StaminaCost = 10.0f;
	F.Character->SetRole(ROLE_AutonomousProxy);
	TestEqual(TEXT("Client reads the server cost rather than its local seed"), F.Equipment->GetWeaponStaminaCost(), 2.0f);
	F.Character->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_PR29SprintTest, "FallenEra.Combat.PR29.SprintStateValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_PR29SprintTest::RunTest(const FString& Parameters)
{
	FFE_PR29Fixture F;
	UFE_PlayerMovementStatsComponent* Movement = NewObject<UFE_PlayerMovementStatsComponent>(F.Character);
	Movement->RegisterComponent();
	TestTrue(TEXT("Sprint RPC includes validation"), Movement->FindFunction(TEXT("ServerSetSprinting"))->HasAnyFunctionFlags(FUNC_NetValidate));
	Movement->SetSprinting(true);
	TestTrue(TEXT("Ordinary sprint starts"), Movement->IsSprinting());
	const TArray<FGameplayTag> BlockingTags = { FallenEraCombatGameplayTags::State_Combat_Aiming, FallenEraGameplayTags::State_Stunned, FallenEraGameplayTags::State_Dead };
	for (FGameplayTag Tag : BlockingTags)
	{
		F.ASC->AddLooseGameplayTag(Tag);
		Movement->SetSprinting(true);
		TestFalse(TEXT("Server refuses sprint while gameplay state blocks it"), Movement->IsSprinting());
		F.ASC->RemoveLooseGameplayTag(Tag);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_PR29SlowTest, "FallenEra.Combat.PR29.TagSlowUsesGASImmediately", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_PR29SlowTest::RunTest(const FString& Parameters)
{
	FFE_PR29Fixture F;
	AFE_EnemySettingsTestCharacter* Enemy = F.World->SpawnActor<AFE_EnemySettingsTestCharacter>();
	Enemy->InitializeComponents();
	AFE_EnemyObstacleTestController* Controller = F.World->SpawnActor<AFE_EnemyObstacleTestController>();
	Controller->Possess(Enemy);
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	const float BaseSpeed = Enemy->GetCharacterMovement()->MaxWalkSpeed;
	ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Slowed);
	TestEqual(TEXT("Slow changes a GAS attribute"), ASC->GetNumericAttribute(UFallenEraAttributeSet::GetMovementSpeedMultiplierAttribute()), 0.5f);
	TestEqual(TEXT("Speed changes without waiting for the decision timer"), Enemy->GetCharacterMovement()->MaxWalkSpeed, BaseSpeed * 0.5f);
	ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Slowed);
	ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Slowed);
	TestEqual(TEXT("Remaining slow source keeps one effect"), Enemy->GetCharacterMovement()->MaxWalkSpeed, BaseSpeed * 0.5f);
	ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Slowed);
	TestEqual(TEXT("Last source removal restores the original factor"), Enemy->GetCharacterMovement()->MaxWalkSpeed, BaseSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_PR29ReloadTest, "FallenEra.Combat.PR29.ReloadLateReconstruction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_PR29ReloadTest::RunTest(const FString& Parameters)
{
	FFE_PR29Fixture F;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Combat/Character/PlayerCharacter/Animations/Bow/AM_BowRelease.AM_BowRelease"));
	if (!TestNotNull(TEXT("Character test mesh"), Mesh) || !TestNotNull(TEXT("Test montage"), Montage)) { return false; }
	F.Character->GetMesh()->SetSkeletalMeshAsset(Mesh);
	F.Character->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());
	UAnimInstance* Anim = F.Character->GetMesh()->GetAnimInstance();
	if (!TestNotNull(TEXT("Native animation instance"), Anim)) { return false; }
	UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(F.Character);
	Combat->RegisterComponent();
	Combat->StartReloadPresentation(Montage, nullptr, Montage->GetPlayLength() / Montage->RateScale);
	TestTrue(TEXT("Server presentation begins"), Anim->Montage_IsActive(Montage));
	F.World->Tick(LEVELTICK_TimeOnly, 0.1f);
	Anim->Montage_Stop(0.0f, Montage);
	F.Character->SetRole(ROLE_SimulatedProxy);
	Combat->ProcessEvent(Combat->FindFunction(TEXT("OnRep_ReloadState")), nullptr);
	TestTrue(TEXT("Newly relevant observer restores active reload"), Anim->Montage_IsActive(Montage));
	TestEqual(TEXT("Reload resumes at elapsed server time, not frame zero"), Anim->Montage_GetPosition(Montage), static_cast<float>(F.World->GetTimeSeconds()) * Montage->RateScale);
	F.Character->SetRole(ROLE_Authority);
	Combat->EndReloadPresentation();
	F.Character->SetRole(ROLE_SimulatedProxy);
	Combat->ProcessEvent(Combat->FindFunction(TEXT("OnRep_ReloadState")), nullptr);
	TestFalse(TEXT("Cancelled state cannot replay a late asset callback"), Anim->Montage_IsActive(Montage));
	F.Character->SetRole(ROLE_Authority);
	return true;
}

#endif
