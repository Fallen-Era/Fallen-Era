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
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
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
	TestTrue(TEXT("TP body disables the renderer's forced owner-hiding mode"),
		Player->GetMesh()->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
	TestTrue(TEXT("TP weapon and attachment use ordinary world rendering"),
		WorldWeapon->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None && WorldSight->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
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
	Sight->BlendOutTime = 0.2f;
	Sight->AimFieldOfView = 50.0f;
	UStaticMesh* WeaponMesh = NewObject<UStaticMesh>(Player);
	UStaticMeshSocket* AimSocket = NewObject<UStaticMeshSocket>(WeaponMesh);
	AimSocket->SocketName = Sight->AimSocketName;
	WeaponMesh->Sockets.Add(AimSocket);
	FirstWeapon->SetStaticMesh(WeaponMesh);
	Weapon->Template.bSupportsAiming = true;
	Weapon->Template.DefaultSight = Sight;
	FindFProperty<FObjectPropertyBase>(Equipment->GetClass(), TEXT("CurrentWeaponData"))->SetObjectPropertyValue_InContainer(Equipment, Weapon);
	TestTrue(TEXT("Initialize sight through equipment public API"), Equipment->SetWeaponAttachments({}));
	UFE_WeaponAimingComponent* Aiming = Player->GetWeaponAimingComponent();
	const float OriginalThirdFOV = Third->FieldOfView;
	const float OriginalFirstFOV = First->FieldOfView;
	AimSocket->SocketName = TEXT("MissingAimSocket");
	TestFalse(TEXT("TP ADS also validates the first-person optic socket"), Aiming->BeginAim());
	TestTrue(TEXT("Failed ADS leaves third-person view untouched"), Player->IsThirdPersonView());
	AimSocket->SocketName = Sight->AimSocketName;
	TestTrue(TEXT("Third-person ADS enters first-person aim"), Aiming->BeginAim());
	TestTrue(TEXT("ADS prepares FP aiming but keeps the outgoing camera active until the cut"),
		!First->IsActive() && Third->IsActive() && IFE_CombatPresentation::UsesFirstPersonView(Player));
	TestFalse(TEXT("FP aiming selection does not reveal FP meshes before camera arrival"), IFE_CombatPresentation::UsesFirstPersonMeshes(Player));
	TestTrue(TEXT("Outgoing TP body, weapon and attachments remain visible when ADS starts"),
		!Player->GetMesh()->bOwnerNoSee && !WorldWeapon->bOwnerNoSee && !WorldSight->bOwnerNoSee &&
		Player->GetMesh()->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
	TestTrue(TEXT("Incoming arms, legs and FP weapon stay owner-hidden"),
		Player->GetFirstPersonMesh()->bOwnerNoSee && Player->GetFirstPersonLegsMesh()->bOwnerNoSee && FirstWeapon->bOwnerNoSee);
	FMinimalViewInfo AimView;
	Player->CalcCamera(0.0f, AimView);
	TestTrue(TEXT("ADS begins at outgoing third-person location, not a camera cut"), AimView.Location.Equals(ThirdView.Location, 0.001f));
	Aiming->TickComponent(0.1f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.1f, AimView);
	const float ApproachDistance = FindFProperty<FFloatProperty>(PlayerClass, TEXT("AimCameraApproachDistance"))->GetPropertyValue_InContainer(Player);
	const float SwitchAlpha = FindFProperty<FFloatProperty>(PlayerClass, TEXT("AimCameraSwitchAlpha"))->GetPropertyValue_InContainer(Player);
	const FVector ApproachDirection = (First->GetComponentLocation() - Third->GetComponentLocation()).GetSafeNormal();
	TestTrue(TEXT("ADS only dollies a short distance from the third-person camera"),
		AimView.Location.Equals(Third->GetComponentLocation() + ApproachDirection * ApproachDistance *
			FMath::SmoothStep(0.0f, SwitchAlpha, 0.5f), 0.001f));
	TestTrue(TEXT("ADS changes FP FOV and preserves the shoulder camera baseline"), Third->FieldOfView == OriginalThirdFOV && First->FieldOfView < OriginalFirstFOV);
	TestFalse(TEXT("TP ADS uses first-person socket alignment"), Player->GetFirstPersonMesh()->GetRelativeTransform().Equals(ArmsTransform));
	Equipment->RefreshPerspectivePresentation();
	TestTrue(TEXT("Mid-transition equipment refresh keeps world meshes visible and FP meshes hidden"),
		!WorldWeapon->bOwnerNoSee && !WorldSight->bOwnerNoSee && FirstWeapon->bOwnerNoSee &&
		!IFE_CombatPresentation::UsesFirstPersonMeshes(Player));
	Controller->SetControlRotation(FRotator(10.0f, 20.0f, 0.0f));
	Arm->TickComponent(0.0f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.0f, AimView);
	TestTrue(TEXT("Hidden ADS camera tracks look input before the late cut"),
		First->GetComponentRotation().Equals(Controller->GetControlRotation(), 0.001f));
	// Release and repress mid-transition must reverse continuously, not restart at either endpoint.
	Aiming->EndAim();
	FMinimalViewInfo ReleasedView;
	Player->CalcCamera(0.0f, ReleasedView);
	TestTrue(TEXT("Release keeps the current camera position"), ReleasedView.Location.Equals(AimView.Location, 0.001f));
	Aiming->TickComponent(0.05f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.05f, ReleasedView);
	TestTrue(TEXT("Release starts returning to third person immediately"),
		FVector::Dist(ReleasedView.Location, Third->GetComponentLocation()) < FVector::Dist(AimView.Location, Third->GetComponentLocation()));
	TestTrue(TEXT("Quick repress resumes ADS"), Aiming->BeginAim());
	Player->CalcCamera(0.0f, AimView);
	TestTrue(TEXT("Repress does not snap the camera"), AimView.Location.Equals(ReleasedView.Location, 0.001f));
	TestTrue(TEXT("Early release/repress cannot flash FP meshes"), Player->GetFirstPersonMesh()->bOwnerNoSee && !WorldWeapon->bOwnerNoSee);
	Aiming->TickComponent(0.116f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.116f, AimView);
	TestTrue(TEXT("Near the cut the camera still stays outside the body with TP meshes"),
		Third->IsActive() && !First->IsActive() && !IFE_CombatPresentation::UsesFirstPersonMeshes(Player) &&
		FVector::Dist(AimView.Location, First->GetComponentLocation()) >= Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 20.0f);
	Aiming->TickComponent(0.006f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.006f, AimView);
	TestTrue(TEXT("Late transition cuts camera and meshes to FP together before full duration"),
		First->IsActive() && !Third->IsActive() && IFE_CombatPresentation::UsesFirstPersonMeshes(Player) &&
		AimView.Location.Equals(First->GetComponentLocation(), 0.001f));
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.2f, AimView);
	TestTrue(TEXT("Holding aim reaches the FP endpoint"), AimView.Location.Equals(First->GetComponentLocation(), 0.001f));
	TestTrue(TEXT("Camera arrival reveals arms, legs and FP weapon together"),
		IFE_CombatPresentation::UsesFirstPersonMeshes(Player) && !Player->GetFirstPersonMesh()->bOwnerNoSee &&
		!Player->GetFirstPersonLegsMesh()->bOwnerNoSee && !FirstWeapon->bOwnerNoSee);
	TestTrue(TEXT("Camera arrival hides the world body, weapon and attachments"),
		Player->GetMesh()->bOwnerNoSee && WorldWeapon->bOwnerNoSee && WorldSight->bOwnerNoSee &&
		WorldWeapon->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	TestFalse(TEXT("Fully aimed FP does not tick the unused spring arm"), Arm->IsComponentTickEnabled());
	Aiming->EndAim();
	TestTrue(TEXT("Leaving ADS immediately restores world meshes to avoid floating FP arms"),
		!IFE_CombatPresentation::UsesFirstPersonMeshes(Player) && Player->GetFirstPersonMesh()->bOwnerNoSee &&
		FirstWeapon->bOwnerNoSee && !Player->GetMesh()->bOwnerNoSee && !WorldWeapon->bOwnerNoSee);
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.2f, ReleasedView);
	TestTrue(TEXT("Release restores the original TP choice"), Player->IsThirdPersonView() && ReleasedView.Location.Equals(Third->GetComponentLocation(), 0.001f));
	TestEqual(TEXT("Release restores first-person FOV"), First->FieldOfView, OriginalFirstFOV);
	TestTrue(TEXT("Return enables third-person spring arm again"), Arm->IsComponentTickEnabled());
	// A wall-shortened boom must not cause the approach animation to enter the capsule.
	const float OriginalArmLength = Arm->TargetArmLength;
	Arm->TargetArmLength = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 30.0f;
	Arm->TickComponent(0.0f, LEVELTICK_All, nullptr);
	Aiming->BeginAim();
	Aiming->TickComponent(0.16f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.16f, AimView);
	TestTrue(TEXT("Short boom approach is limited by body clearance instead of crossing it"),
		Third->IsActive() && FVector::Dist(AimView.Location, First->GetComponentLocation()) >=
		Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 20.0f - 0.001f);
	Aiming->EndAim(true);
	Player->CalcCamera(0.2f, ReleasedView);
	Arm->TargetArmLength = OriginalArmLength;
	Arm->TickComponent(0.0f, LEVELTICK_All, nullptr);
	Aiming->BeginAim();
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	Player->TogglePerspective();
	TestFalse(TEXT("Switching camera ends captured ADS"), Aiming->IsAiming());
	TestEqual(TEXT("Switching camera restores previous FOV"), Third->FieldOfView, OriginalThirdFOV);
	TestTrue(TEXT("Return activates only first-person camera"), First->IsActive() && !Third->IsActive());
	TestTrue(TEXT("Return restores owner-only arms and world hiding"), !Player->GetFirstPersonMesh()->bOwnerNoSee && Player->GetMesh()->bOwnerNoSee);
	TestTrue(TEXT("Return restores first-person world-shadow representations"),
		Player->GetMesh()->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation &&
		WorldWeapon->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation &&
		WorldSight->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	TestTrue(TEXT("Return restores weapon/attachment flags"), WorldWeapon->bOwnerNoSee && WorldSight->bOwnerNoSee && !FirstWeapon->bOwnerNoSee);
	TestTrue(TEXT("View switching does not drift arm or leg transforms"), Player->GetFirstPersonMesh()->GetRelativeTransform().Equals(ArmsTransform) &&
		Player->GetFirstPersonLegsMesh()->GetRelativeTransform().Equals(LegsTransform));
	TestFalse(TEXT("Returning to FP stops spring-arm tick"), Arm->IsComponentTickEnabled());
	TestTrue(TEXT("Normal first-person ADS can still begin"), Aiming->BeginAim());
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	Aiming->EndAim(true);
	Player->CalcCamera(0.2f, AimView);
	TestTrue(TEXT("First-person ADS release never forces third person"), !Player->IsThirdPersonView() && AimView.Location.Equals(First->GetComponentLocation(), 0.001f));
	Player->SetThirdPersonView(true);
	Aiming->BeginAim();
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	Player->CalcCamera(0.2f, AimView);
	Aiming->EndAim(true);
	Player->CalcCamera(0.1f, ReleasedView);
	TestTrue(TEXT("Forced cancellation restores TP with a camera blend, not a cut"), Player->IsThirdPersonView() &&
		!ReleasedView.Location.Equals(Third->GetComponentLocation(), 0.001f) && !ReleasedView.Location.Equals(First->GetComponentLocation(), 0.001f));
	Player->CalcCamera(0.1f, ReleasedView);
	// Server-side execution does not run local presentation ticks, but must use FP attack aiming during ADS.
	Controller->UnPossess();
	TestTrue(TEXT("Non-local authority can enter ADS"), Aiming->BeginAim());
	TestTrue(TEXT("Authority selects FP aiming too"), IFE_CombatPresentation::UsesFirstPersonView(Player));
	TestFalse(TEXT("Non-local ADS never starts local presentation tick"), Aiming->IsComponentTickEnabled());
	Aiming->EndAim(true);
	TestTrue(TEXT("Authority returns to TP preference"), Player->IsThirdPersonView());

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
