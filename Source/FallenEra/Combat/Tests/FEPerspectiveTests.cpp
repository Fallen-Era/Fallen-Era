#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FallenEraCharacter.h"
#include "Camera/CameraComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/Interface/FECombatPresentation.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/Weapon/Attachment/FEWeaponSightData.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_PerspectiveTest, "FallenEra.Combat.Presentation.PerspectiveSwitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_PerspectiveTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UClass* PlayerClass = LoadClass<AFallenEraCharacter>(nullptr,
		TEXT("/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter.BP_HT_PlayerCharacter_C"));
	if (!TestNotNull(TEXT("Player Blueprint"), PlayerClass))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	AFallenEraCharacter* Player = World->SpawnActor<AFallenEraCharacter>(PlayerClass);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Player);
	UCameraComponent* First = Player->GetFirstPersonCameraComponent();
	UCameraComponent* Third = Player->GetThirdPersonCameraComponent();
	USpringArmComponent* Arm = Player->GetThirdPersonSpringArm();
	TestTrue(TEXT("Spring arm is attached to capsule"), Arm->GetAttachParent() == Player->GetCapsuleComponent());
	TestTrue(TEXT("Third camera uses spring arm"), Third->GetAttachParent() == Arm);
	TestTrue(TEXT("Third camera has wall collision"), Arm->bDoCollisionTest);
	TestTrue(TEXT("Default is first person"), First->IsActive() && !Third->IsActive() && !Player->IsThirdPersonView());
	TestFalse(TEXT("No inactive spring-arm ticking"), Arm->IsComponentTickEnabled());
	FMinimalViewInfo FirstView;
	Player->CalcCamera(0.0f, FirstView);
	const FTransform ArmsTransform = Player->GetFirstPersonMesh()->GetRelativeTransform();
	const FTransform LegsTransform = Player->GetFirstPersonLegsMesh()->GetRelativeTransform();

	UFE_EquipmentComponent* Equipment = Player->GetEquipmentComponent();
	UStaticMeshComponent* WorldWeapon = NewObject<UStaticMeshComponent>(Player);
	WorldWeapon->SetupAttachment(Player->GetMesh());
	WorldWeapon->RegisterComponent();
	UStaticMeshComponent* FirstWeapon = NewObject<UStaticMeshComponent>(Player);
	FirstWeapon->SetupAttachment(Player->GetFirstPersonMesh());
	FirstWeapon->RegisterComponent();
	UStaticMeshComponent* WorldSight = NewObject<UStaticMeshComponent>(Player);
	WorldSight->SetupAttachment(WorldWeapon);
	WorldSight->RegisterComponent();
	FindFProperty<FObjectPropertyBase>(Equipment->GetClass(), TEXT("EquippedWorldWeaponMesh"))->SetObjectPropertyValue_InContainer(Equipment, WorldWeapon);
	FindFProperty<FObjectPropertyBase>(Equipment->GetClass(), TEXT("EquippedFirstPersonWeaponMesh"))->SetObjectPropertyValue_InContainer(Equipment, FirstWeapon);
	Equipment->RefreshPerspectivePresentation();
	TestTrue(TEXT("FP hides world weapon from owner"), WorldWeapon->bOwnerNoSee);
	Player->TogglePerspective();
	TestTrue(TEXT("Only third camera becomes active"), Third->IsActive() && !First->IsActive());
	TestTrue(TEXT("Presentation interface selects third camera"), IFE_CombatPresentation::FindCombatCamera(Player) == Third);
	TestFalse(TEXT("Presentation interface leaves FP mode"), IFE_CombatPresentation::UsesFirstPersonView(Player));
	TestFalse(TEXT("Full body visible to owner in TP"), Player->GetMesh()->bOwnerNoSee);
	TestTrue(TEXT("FP arms and legs hidden from owner in TP"), Player->GetFirstPersonMesh()->bOwnerNoSee && Player->GetFirstPersonLegsMesh()->bOwnerNoSee);
	TestTrue(TEXT("World weapon and attachment visible in TP"), !WorldWeapon->bOwnerNoSee && !WorldSight->bOwnerNoSee);
	TestTrue(TEXT("FP weapon hidden in TP"), FirstWeapon->bOwnerNoSee && FirstWeapon->bOnlyOwnerSee);
	FMinimalViewInfo ThirdView;
	Player->CalcCamera(0.0f, ThirdView);
	TestTrue(TEXT("Third camera is behind the first-person viewpoint"), FVector::Distance(ThirdView.Location, FirstView.Location) > 100.0f);
	TestTrue(TEXT("Only active third-person spring arm ticks"), Arm->IsComponentTickEnabled());
	// Asynchronously created equipment must inherit the currently selected view too.
	Equipment->RefreshPerspectivePresentation();
	TestFalse(TEXT("Equipment refresh cannot rehide TP weapon"), WorldWeapon->bOwnerNoSee);
	UFE_RangedWeaponItemData* Weapon = NewObject<UFE_RangedWeaponItemData>(Player);
	UFE_WeaponSightData* Sight = NewObject<UFE_WeaponSightData>(Player);
	Sight->BlendInTime = 0.2f;
	Sight->AimFieldOfView = 50.0f;
	Weapon->Template.bSupportsAiming = true;
	Weapon->Template.DefaultSight = Sight;
	FindFProperty<FObjectPropertyBase>(Equipment->GetClass(), TEXT("CurrentWeaponData"))->SetObjectPropertyValue_InContainer(Equipment, Weapon);
	TestTrue(TEXT("Initialize sight through equipment public API"), Equipment->SetWeaponAttachments({}));
	UFE_WeaponAimingComponent* Aiming = Player->GetWeaponAimingComponent();
	const float OriginalThirdFOV = Third->FieldOfView;
	const float OriginalFirstFOV = First->FieldOfView;
	TestTrue(TEXT("Third-person shoulder aim can begin"), Aiming->BeginAim());
	Aiming->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Third-person aim changes only the active camera FOV"), Third->FieldOfView < OriginalThirdFOV && First->FieldOfView == OriginalFirstFOV);
	TestTrue(TEXT("Third-person aim never aligns hidden arms to the shoulder camera"), Player->GetFirstPersonMesh()->GetRelativeTransform().Equals(ArmsTransform));
	Player->TogglePerspective();
	TestFalse(TEXT("Switching camera ends captured ADS"), Aiming->IsAiming());
	TestEqual(TEXT("Switching camera restores previous FOV"), Third->FieldOfView, OriginalThirdFOV);
	TestTrue(TEXT("Return activates only first-person camera"), First->IsActive() && !Third->IsActive());
	TestTrue(TEXT("Return restores owner-only arms and world hiding"), !Player->GetFirstPersonMesh()->bOwnerNoSee && Player->GetMesh()->bOwnerNoSee);
	TestTrue(TEXT("Return restores weapon/attachment flags"), WorldWeapon->bOwnerNoSee && WorldSight->bOwnerNoSee && !FirstWeapon->bOwnerNoSee);
	TestTrue(TEXT("View switching does not drift arm or leg transforms"), Player->GetFirstPersonMesh()->GetRelativeTransform().Equals(ArmsTransform) &&
		Player->GetFirstPersonLegsMesh()->GetRelativeTransform().Equals(LegsTransform));
	TestFalse(TEXT("Returning to FP stops spring-arm tick"), Arm->IsComponentTickEnabled());

	const AFallenEraCharacter* Defaults = PlayerClass->GetDefaultObject<AFallenEraCharacter>();
	const UInputAction* Action = Cast<UInputAction>(FindFProperty<FObjectPropertyBase>(PlayerClass, TEXT("CombatTogglePerspectiveAction"))->GetObjectPropertyValue_InContainer(Defaults));
	const UInputMappingContext* Context = Cast<UInputMappingContext>(FindFProperty<FObjectPropertyBase>(PlayerClass, TEXT("CombatMappingContext"))->GetObjectPropertyValue_InContainer(Defaults));
	TestTrue(TEXT("Perspective action is assigned and Boolean"), Action && Action->ValueType == EInputActionValueType::Boolean);
	TestTrue(TEXT("Combat context contains V perspective binding"), Context && Context->GetMappings().ContainsByPredicate(
		[Action](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action && Mapping.Key == EKeys::V; }));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
