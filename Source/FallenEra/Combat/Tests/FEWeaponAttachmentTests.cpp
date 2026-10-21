#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEPlayerMovementStatsComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/Weapon/Attachment/FEWeaponSightData.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "FallenEraCharacter.h"
#include "FallenEraPlayerState.h"
#include "Combat/FECombatGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "Combat/Ability/FEWeaponAimAbility.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_WeaponAttachmentTest, "FallenEra.Combat.Attachments.InstanceAndStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_WeaponAttachmentTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	ACharacter* Character = World->SpawnActor<ACharacter>();
	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Character);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Character, Character);
	UFallenEraAttributeSet* Stats = NewObject<UFallenEraAttributeSet>(Character);
	ASC->AddAttributeSetSubobject(Stats);
	Stats->InitAccuracy(0.7f);
	Stats->InitAttackPower(10.0f);
	Stats->InitRecoilControl(0.6f);
	UFE_EquipmentComponent* Equipment = NewObject<UFE_EquipmentComponent>(Character);
	Equipment->RegisterComponent();
	UFE_RangedWeaponItemData* Weapon = NewObject<UFE_RangedWeaponItemData>();
	Weapon->Template.bSupportsAiming = true;
	UFE_WeaponSightData* Irons = NewObject<UFE_WeaponSightData>();
	Irons->AccuracyBonus = 0.05f;
	Weapon->Template.DefaultSight = Irons;
	FindFProperty<FObjectPropertyBase>(UFE_EquipmentComponent::StaticClass(), TEXT("CurrentWeaponData"))
		->SetObjectPropertyValue_InContainer(Equipment, Weapon);
	FindFProperty<FFloatProperty>(UFE_EquipmentComponent::StaticClass(), TEXT("EquipmentChangeInterval"))
		->SetPropertyValue_InContainer(Equipment, 0.0f);

	UFE_WeaponSightData* RedDot = NewObject<UFE_WeaponSightData>();
	RedDot->Mode = EFE_WeaponSightMode::RedDot;
	RedDot->AccuracyBonus = 0.15f;
	FFE_WeaponAttachmentInstance Entry;
	Entry.Definition = RedDot;
	TestEqual(TEXT("DA stat resolves"), Entry.ResolveAccuracyBonus(), 0.15f);
	TestTrue(TEXT("Equip loaded sight"), Equipment->SetWeaponAttachments({Entry}));
	TestTrue(TEXT("Loaded parts ready"), Equipment->AreWeaponAttachmentsReady());
	TestTrue(TEXT("Mounted sight overrides built-in irons"), Equipment->GetActiveSightData() == RedDot);
	TestTrue(TEXT("Mounted sight bonus replaces, not stacks with, built-in bonus"), FMath::IsNearlyEqual(Stats->GetAccuracy(), 0.85f));
	TestEqual(TEXT("Part does not change offense"), Stats->GetAttackPower(), 10.0f);
	TestEqual(TEXT("Part does not change recoil"), Stats->GetRecoilControl(), 0.6f);

	Entry.bOverrideAccuracyBonus = true;
	Entry.AccuracyBonus = 0.25f;
	TestTrue(TEXT("Instance replaces part stats"), Equipment->SetWeaponAttachments({Entry}));
	TestTrue(TEXT("Replacement removes old bonus, does not stack"), FMath::IsNearlyEqual(Stats->GetAccuracy(), 0.95f));
	TestEqual(TEXT("Shared DA remains unchanged"), RedDot->AccuracyBonus, 0.15f);
	TestEqual(TEXT("Instance override wins"), Entry.ResolveAccuracyBonus(), 0.25f);
	Entry.AccuracyBonus = 5.0f;
	TestEqual(TEXT("Instance values are bounded"), Entry.ResolveAccuracyBonus(), 1.0f);

	UFE_WeaponAimingComponent* Aiming = NewObject<UFE_WeaponAimingComponent>(Character);
	Aiming->RegisterComponent();
	UFE_PlayerMovementStatsComponent* Movement = NewObject<UFE_PlayerMovementStatsComponent>(Character);
	Movement->RegisterComponent();
	Movement->SetSprinting(true);
	TestTrue(TEXT("Sprint allowed before ADS"), Movement->IsSprinting());
	TestTrue(TEXT("Authority accepts configured sight"), Aiming->BeginAim());
	TestTrue(TEXT("Aim state starts"), Aiming->IsAiming());
	TestFalse(TEXT("Authority ADS cancels sprint"), Movement->IsSprinting());
	TestEqual(TEXT("Authority ADS restores walk speed"), Character->GetCharacterMovement()->MaxWalkSpeed, 300.0f);
	Movement->SetSprinting(true);
	TestFalse(TEXT("Authority rejects sprint while aiming"), Movement->IsSprinting());
	TestFalse(TEXT("Non-local actor never ticks ADS rendering"), Aiming->IsComponentTickEnabled());
	Aiming->ApplyShotRecoil(Weapon->Template.VisualRecoilSettings, 0.0f);
	TestTrue(TEXT("Remote characters do not accumulate local visual recoil"),
		Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity));
	TestTrue(TEXT("Removing mounted sight"), Equipment->SetWeaponAttachments({}));
	TestFalse(TEXT("Replacing parts cancels active or blending-out ADS"), Aiming->IsAiming());
	TestTrue(TEXT("Built-in irons restored"), Equipment->GetActiveSightData() == Irons);
	TestTrue(TEXT("Built-in sight bonus applies without any mounted parts"), FMath::IsNearlyEqual(Stats->GetAccuracy(), 0.75f));
	TestTrue(TEXT("Refreshing built-in sight"), Equipment->SetWeaponAttachments({}));
	TestTrue(TEXT("Built-in bonus does not accumulate on refresh"), FMath::IsNearlyEqual(Stats->GetAccuracy(), 0.75f));
	Aiming->EndAim(true);
	TestFalse(TEXT("Aim ends"), Aiming->IsAiming());
	TestEqual(TEXT("End resets aim alpha"), Aiming->GetAimAlpha(), 0.0f);
	Movement->SetSprinting(true);
	TestTrue(TEXT("Sprint allowed after ADS ends"), Movement->IsSprinting());
	TestEqual(TEXT("Sprint speed restored on new input"), Character->GetCharacterMovement()->MaxWalkSpeed, 600.0f);

	Entry.Definition = Irons;
	Entry.AccuracyBonus = 0.08f;
	TestTrue(TEXT("Same DA can be mounted with instance stats"), Equipment->SetWeaponAttachments({Entry}));
	TestTrue(TEXT("Shared default/mounted DA still applies only the instance bonus"),
		FMath::IsNearlyEqual(Stats->GetAccuracy(), 0.78f));
	TestEqual(TEXT("Built-in DA remains immutable"), Irons->AccuracyBonus, 0.05f);
	UFE_WeaponAttachmentData* OtherPart = NewObject<UFE_WeaponAttachmentData>();
	OtherPart->SlotTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Body"));
	OtherPart->AccuracyBonus = 0.02f;
	FFE_WeaponAttachmentInstance OtherEntry;
	OtherEntry.Definition = OtherPart;
	TestTrue(TEXT("Equip non-sight part"), Equipment->SetWeaponAttachments({OtherEntry}));
	TestTrue(TEXT("Built-in sight and independent part bonuses combine"), FMath::IsNearlyEqual(Stats->GetAccuracy(), 0.77f));
	Equipment->RefreshEquipment();
	TestTrue(TEXT("Clearing weapon removes built-in and part bonuses"), FMath::IsNearlyEqual(Stats->GetAccuracy(), 0.7f));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_FirstPersonWeaponTransformTest, "FallenEra.Combat.Attachments.FirstPersonTransforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_FirstPersonWeaponTransformTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UClass* CharacterClass = LoadClass<AFallenEraCharacter>(nullptr,
		TEXT("/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter.BP_HT_PlayerCharacter_C"));
	if (!TestNotNull(TEXT("Concrete player blueprint"), CharacterClass))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	AFallenEraCharacter* Character = World->SpawnActor<AFallenEraCharacter>(CharacterClass);
	if (!TestNotNull(TEXT("Player spawns"), Character))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->PlayerState = World->SpawnActor<AFallenEraPlayerState>();
	World->InitializeActorsForPlay(FURL());
	Controller->Possess(Character);
	TestTrue(TEXT("Test player is locally controlled"), Character->IsLocallyControlled());
	USkeletalMeshComponent* Arms = Character->GetFirstPersonMesh();
	UFE_EquipmentComponent* Equipment = Character->FindComponentByClass<UFE_EquipmentComponent>();
	UFE_WeaponAimingComponent* Aiming = Character->FindComponentByClass<UFE_WeaponAimingComponent>();
	UFE_PlayerMovementStatsComponent* Movement = Character->FindComponentByClass<UFE_PlayerMovementStatsComponent>();
	const FTransform UnequippedTransform = Arms->GetRelativeTransform();
	const float OriginalFOV = Character->GetFirstPersonCameraComponent()->FieldOfView;
	const float OriginalFirstPersonFOV = Character->GetFirstPersonCameraComponent()->FirstPersonFieldOfView;
	UFE_WeaponSightData* Sight = NewObject<UFE_WeaponSightData>();
	Sight->BlendInTime = Sight->BlendOutTime = 0.2f;
	UFE_RangedWeaponItemData* Weapon = NewObject<UFE_RangedWeaponItemData>();
	Weapon->Template.bSupportsAiming = true;
	Weapon->Template.AimSwaySettings.bEnabled = false;
	Weapon->Template.DefaultSight = Sight;
	Weapon->Template.ItemTags.AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
	Weapon->Template.FirstPersonMeshTransform = FTransform(FRotator(0, -90, 5), FVector(4, 8, -154));
	UStaticMesh* WeaponMeshAsset = NewObject<UStaticMesh>();
	UStaticMeshSocket* AimSocket = NewObject<UStaticMeshSocket>(WeaponMeshAsset);
	AimSocket->SocketName = Sight->AimSocketName;
	AimSocket->RelativeLocation = FVector(20, 4, 8);
	AimSocket->RelativeRotation = FRotator(5, 10, 0);
	WeaponMeshAsset->Sockets.Add(AimSocket);
	Weapon->Template.Mesh = WeaponMeshAsset;
	UFE_RangedWeaponItemData* SecondWeapon = NewObject<UFE_RangedWeaponItemData>();
	SecondWeapon->Template.bSupportsAiming = true;
	SecondWeapon->Template.DefaultSight = Sight;
	SecondWeapon->Template.Mesh = WeaponMeshAsset;
	SecondWeapon->Template.ItemTags.AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick);
	SecondWeapon->Template.FirstPersonMeshTransform = FTransform(FRotator(0, -90, 0), FVector(2, 10, -150));
	*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("TestItemTags"))
		->ContainerPtrToValuePtr<TArray<FGameplayTag>>(Equipment) = {
			FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick, FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick };
	*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("WeaponDataAssets"))
		->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UFE_WeaponItemData>>>(Equipment) = { Weapon, SecondWeapon };
	FindFProperty<FFloatProperty>(Equipment->GetClass(), TEXT("EquipmentChangeInterval"))
		->SetPropertyValue_InContainer(Equipment, 0.0f);
	Equipment->EquipWeaponByItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
	if (!TestTrue(TEXT("Weapon loaded"), Equipment->GetCurrentWeaponData() == Weapon))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	TestTrue(TEXT("Equipping applies weapon arms base transform"), Arms->GetRelativeTransform().Equals(Weapon->Template.FirstPersonMeshTransform));
	TestTrue(TEXT("Default sight uses weapon mesh"), Equipment->GetFirstPersonSightMesh() == Equipment->GetEquippedFirstPersonWeaponMesh());
	AimSocket->SocketName = TEXT("MissingAimSocket");
	TestFalse(TEXT("Missing aim socket rejects 3D ADS"), Aiming->CanAim());
	AimSocket->SocketName = Sight->AimSocketName;
	const FTransform SightInArms = Equipment->GetFirstPersonSightMesh()->GetSocketTransform(Sight->AimSocketName)
		.GetRelativeTransform(Arms->GetComponentTransform());
	const FTransform DesiredSight(FVector(Sight->EyeReliefDistance, 0, 0));
	const FTransform ExpectedAim = UFE_WeaponAimingComponent::CalculateAlignedArmsTransform(
		SightInArms, DesiredSight, Weapon->Template.FirstPersonMeshTransform.GetScale3D());
	Movement->SetSprinting(true);
	TestTrue(TEXT("Local sprint before ADS"), Movement->IsSprinting());
	TestTrue(TEXT("Local owner aims with the weapon aim socket"), Aiming->BeginAim());
	TestFalse(TEXT("Local ADS immediately cancels sprint"), Movement->IsSprinting());
	Movement->SetSprinting(true);
	TestFalse(TEXT("Local sprint input ignored during ADS"), Movement->IsSprinting());
	Aiming->TickComponent(0.1f, LEVELTICK_All, nullptr);
	FTransform Halfway;
	Halfway.Blend(Weapon->Template.FirstPersonMeshTransform, ExpectedAim, 0.5f);
	TestTrue(TEXT("ADS interpolates arms toward socket alignment"), Arms->GetRelativeTransform().Equals(Halfway, 0.001f));
	TestTrue(TEXT("Holding 3D ADS enables socket correction"), Aiming->IsComponentTickEnabled());
	TestEqual(TEXT("Halfway aim alpha"), Aiming->GetAimAlpha(), 0.5f);
	TestEqual(TEXT("Halfway FOV"), Character->GetFirstPersonCameraComponent()->FieldOfView, FMath::Lerp(OriginalFOV, Sight->AimFieldOfView, 0.5f));
	TestTrue(TEXT("Weapon stays attached to arms with its socket offset"),
		Equipment->GetEquippedFirstPersonWeaponMesh()->GetRelativeTransform().Equals(Weapon->Template.SocketOffset));
	TestTrue(TEXT("ADS does not alter world weapon attachment"),
		Equipment->GetEquippedWorldWeaponMesh()->GetRelativeTransform().Equals(Weapon->Template.SocketOffset));
	Aiming->EndAim();
	Aiming->TickComponent(0.05f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Release ends aim state while blending out"), Aiming->IsAiming());
	TestTrue(TEXT("Repress during blend-out resumes ADS"), Aiming->BeginAim());
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Repress aligns without recapturing a shifted base"), Arms->GetRelativeTransform().Equals(ExpectedAim, 0.001f));
	TestTrue(TEXT("Socket is centered in camera space"), Equipment->GetFirstPersonSightMesh()->GetSocketTransform(Sight->AimSocketName)
		.GetRelativeTransform(Character->GetFirstPersonCameraComponent()->GetComponentTransform()).GetLocation().Equals(DesiredSight.GetLocation(), 0.001f));
	const FTransform StableAim = Arms->GetRelativeTransform();
	TestTrue(TEXT("Repeated held input is accepted"), Aiming->BeginAim());
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Repeated alignment does not drift the cached base"), Arms->GetRelativeTransform().Equals(StableAim, 0.001f));
	FFE_WeaponVisualRecoilSettings VisualRecoil;
	VisualRecoil.KickTranslation = FVector(-1.0f, 0.0f, 0.0f);
	VisualRecoil.KickRotation = FRotator(0.6f, 0.0f, 0.0f);
	const FRotator ViewBeforeVisualRecoil = Controller->GetControlRotation();
	const FTransform CameraBeforeVisualRecoil = Character->GetFirstPersonCameraComponent()->GetRelativeTransform();
	const float FOVBeforeVisualRecoil = Character->GetFirstPersonCameraComponent()->FieldOfView;
	const FTransform WorldWeaponBeforeVisualRecoil = Equipment->GetEquippedWorldWeaponMesh()->GetComponentTransform();
	Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
	TestTrue(TEXT("Shot adds spring velocity without snapping the arms"), Arms->GetRelativeTransform().Equals(StableAim));
	Aiming->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	const FTransform FirstVisualKick = Aiming->GetVisualRecoilTransform();
	TestFalse(TEXT("First frame creates a visual kick"), FirstVisualKick.Equals(FTransform::Identity, 0.0001f));
	TestTrue(TEXT("Kick pulls toward the camera"), FirstVisualKick.GetTranslation().X < 0.0f);
	TestTrue(TEXT("Kick lifts the muzzle"), FirstVisualKick.Rotator().Pitch > 0.0f);
	const FTransform KickedSight = Equipment->GetFirstPersonSightMesh()->GetSocketTransform(Sight->AimSocketName)
		.GetRelativeTransform(Character->GetFirstPersonCameraComponent()->GetComponentTransform());
	TestTrue(TEXT("ADS alignment preserves shot offset instead of cancelling it"),
		KickedSight.Equals(DesiredSight * FirstVisualKick, 0.001f));
	TestTrue(TEXT("Visual recoil preserves arms scale"), Arms->GetRelativeScale3D().Equals(StableAim.GetScale3D()));
	TestTrue(TEXT("Visual recoil never moves the world weapon"),
		Equipment->GetEquippedWorldWeaponMesh()->GetComponentTransform().Equals(WorldWeaponBeforeVisualRecoil));
	TestTrue(TEXT("Visual recoil never rotates ballistic aim"), Controller->GetControlRotation().Equals(ViewBeforeVisualRecoil));
	TestTrue(TEXT("Visual recoil never moves or rotates the camera"),
		Character->GetFirstPersonCameraComponent()->GetRelativeTransform().Equals(CameraBeforeVisualRecoil));
	TestEqual(TEXT("Visual recoil never zooms the camera"), Character->GetFirstPersonCameraComponent()->FieldOfView, FOVBeforeVisualRecoil);
	Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
	TestTrue(TEXT("Automatic fire retains the current pose until the next update"),
		Aiming->GetVisualRecoilTransform().Equals(FirstVisualKick));
	for (int32 Shot = 0; Shot < 100; ++Shot) { Aiming->ApplyShotRecoil(VisualRecoil, 0.0f); }
	Aiming->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Rapid fire has a bounded rotation"),
		FMath::Abs(Aiming->GetVisualRecoilTransform().Rotator().Pitch) <= VisualRecoil.KickRotation.Pitch * VisualRecoil.MaxStack + 0.001f);
	for (int32 Frame = 0; Frame < 120; ++Frame) { Aiming->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr); }
	TestTrue(TEXT("Only the cosmetic kick returns to identity"), Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity));
	TestTrue(TEXT("Settled arms return to the exact aligned pose without drift"), Arms->GetRelativeTransform().Equals(StableAim, 0.001f));
	for (const int32 FrameRate : {30, 60, 144})
	{
		Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
		float PeakPitch = 0.0f;
		for (int32 Frame = 0; Frame < FrameRate; ++Frame)
		{
			Aiming->TickComponent(1.0f / FrameRate, LEVELTICK_All, nullptr);
			PeakPitch = FMath::Max(PeakPitch, Aiming->GetVisualRecoilTransform().Rotator().Pitch);
		}
		TestTrue(FString::Printf(TEXT("Single-shot peak remains stable at %d FPS"), FrameRate),
			FMath::IsNearlyEqual(PeakPitch, VisualRecoil.KickRotation.Pitch, 0.06f));
		TestTrue(TEXT("Recoil fully settles at all tested frame rates"), Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity, 0.001f));
	}
	Aiming->ApplyShotRecoil(VisualRecoil, 1.0f);
	Aiming->TickComponent(0.03f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Perfect RecoilControl prevents visual kick"), Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity));
	VisualRecoil.bEnabled = false;
	Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
	Aiming->TickComponent(0.03f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Weapon can disable visual recoil independently"), Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity));
	VisualRecoil.bEnabled = true;
	Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
	Aiming->TickComponent(0.02f, LEVELTICK_All, nullptr);
	Aiming->EndAim();
	Aiming->TickComponent(0.05f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Release blends out rather than instantly dropping the residual kick"),
		Arms->GetRelativeTransform().Equals(Weapon->Template.FirstPersonMeshTransform));
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Release clears residual kick when ADS blend finishes"), Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity));
	TestFalse(TEXT("Finished release has no recoil-only tick"), Aiming->IsComponentTickEnabled());
	Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
	TestFalse(TEXT("Hip fire does not enable ADS presentation tick"), Aiming->IsComponentTickEnabled());
	const FRotator CameraRotationBeforeAngles = Character->GetFirstPersonCameraComponent()->GetRelativeRotation();
	for (const FRotator& ViewAngle : {FRotator(80.0f, 137.0f, 0.0f), FRotator(-80.0f, -137.0f, 0.0f)})
	{
		Character->GetFirstPersonCameraComponent()->SetRelativeRotation(ViewAngle);
		Aiming->BeginAim();
		Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
		Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
		Aiming->TickComponent(0.02f, LEVELTICK_All, nullptr);
		const FTransform AngledSight = Equipment->GetFirstPersonSightMesh()->GetSocketTransform(Sight->AimSocketName)
			.GetRelativeTransform(Character->GetFirstPersonCameraComponent()->GetComponentTransform());
		TestTrue(TEXT("Recoil remains camera-local at steep pitch and yaw"),
			AngledSight.Equals(DesiredSight * Aiming->GetVisualRecoilTransform(), 0.001f));
		Aiming->EndAim(true);
	}
	Character->GetFirstPersonCameraComponent()->SetRelativeRotation(CameraRotationBeforeAngles);
	Aiming->BeginAim();
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	Equipment->GetEquippedFirstPersonWeaponMesh()->SetRelativeLocation(FVector(0, 0, 5));
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Animated/socket offset changes remain centered with sway disabled"), Equipment->GetFirstPersonSightMesh()->GetSocketTransform(Sight->AimSocketName)
		.GetRelativeTransform(Character->GetFirstPersonCameraComponent()->GetComponentTransform()).GetLocation().Equals(DesiredSight.GetLocation(), 0.001f));
	Equipment->GetEquippedFirstPersonWeaponMesh()->SetRelativeTransform(Weapon->Template.SocketOffset);
	Aiming->EndAim();
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Release restores equipped weapon base"), Arms->GetRelativeTransform().Equals(Weapon->Template.FirstPersonMeshTransform));
	TestEqual(TEXT("Release restores FOV"), Character->GetFirstPersonCameraComponent()->FieldOfView, OriginalFOV);
	TestEqual(TEXT("Release restores first-person FOV"), Character->GetFirstPersonCameraComponent()->FirstPersonFieldOfView, OriginalFirstPersonFOV);
	TestFalse(TEXT("Blend-out completion disables ADS tick"), Aiming->IsComponentTickEnabled());
	Weapon->Template.AimSwaySettings.bEnabled = true;
	Weapon->Template.AimSwaySettings.SmoothingTime = 0.0f;
	const FRotator OriginalCameraRotation = Character->GetFirstPersonCameraComponent()->GetRelativeRotation();
	const FRotator OriginalControlRotation = Controller->GetControlRotation();
	Aiming->BeginAim();
	Character->GetFirstPersonCameraComponent()->AddRelativeRotation(FRotator(4, 10, 0));
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	const FTransform Sway = Aiming->GetAimSwayTransform();
	TestFalse(TEXT("Look motion generates intentional sway"), Sway.GetLocation().IsNearlyZero(0.0001f));
	const FTransform SwayedSight = Equipment->GetFirstPersonSightMesh()->GetSocketTransform(Sight->AimSocketName)
		.GetRelativeTransform(Character->GetFirstPersonCameraComponent()->GetComponentTransform());
	TestTrue(TEXT("Socket correction preserves camera-local sway position"),
		SwayedSight.GetLocation().Equals(DesiredSight.GetLocation() + Sway.GetLocation(), 0.001f));
	TestTrue(TEXT("Socket correction preserves camera-local sway rotation"),
		SwayedSight.GetRotation().Equals(Sway.GetRotation(), 0.001f));
	TestTrue(TEXT("Cosmetic sway does not modify ballistic control rotation"),
		Controller->GetControlRotation().Equals(OriginalControlRotation));
	Aiming->EndAim(true);
	TestTrue(TEXT("Cancelling ADS clears sway"), Aiming->GetAimSwayTransform().Equals(FTransform::Identity));
	TestTrue(TEXT("Cancelling ADS restores the equipped base"), Arms->GetRelativeTransform().Equals(Weapon->Template.FirstPersonMeshTransform));
	Character->GetFirstPersonCameraComponent()->SetRelativeRotation(OriginalCameraRotation);
	Aiming->BeginAim();
	Character->GetCharacterMovement()->Velocity = FVector(300, 0, 0);
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Walking generates intentional sway without view input"),
		Aiming->GetAimSwayTransform().GetLocation().IsNearlyZero(0.0001f));
	TestTrue(TEXT("Movement sway leaves ballistic view unchanged"),
		Controller->GetControlRotation().Equals(OriginalControlRotation));
	Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Stationary motion removes movement sway"), Aiming->GetAimSwayTransform().Equals(FTransform::Identity));
	Aiming->EndAim(true);
	Movement->SetSprinting(true);
	TestTrue(TEXT("Local sprint can resume on new input after release"), Movement->IsSprinting());
	UObject* ChargeSource = NewObject<UFE_MeleeAttackData>(Aiming);
	Aiming->BeginChargeSway(ChargeSource, Weapon->Template.AimSwaySettings);
	Character->GetFirstPersonCameraComponent()->AddRelativeRotation(FRotator(0, 10, 0));
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Charge moves arms before entering ADS"), Arms->GetRelativeTransform().Equals(Weapon->Template.FirstPersonMeshTransform, 0.0001f));
	TestTrue(TEXT("ADS can take over from charge sway"), Aiming->BeginAim());
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("ADS takeover retains exact socket alignment"), Equipment->GetFirstPersonSightMesh()->GetSocketTransform(Sight->AimSocketName)
		.GetRelativeTransform(Character->GetFirstPersonCameraComponent()->GetComponentTransform()).GetLocation().Equals(DesiredSight.GetLocation(), 0.001f));
	Aiming->EndAim(true);
	TestTrue(TEXT("Ending ADS resumes an active charge"), Aiming->IsComponentTickEnabled());
	TestTrue(TEXT("Charge resumes from original weapon base, not ADS pose"), Arms->GetRelativeTransform().Equals(Weapon->Template.FirstPersonMeshTransform));
	Aiming->BeginAim();
	Aiming->EndChargeSway(ChargeSource);
	TestTrue(TEXT("Charge release leaves a concurrent ADS active"), Aiming->IsAiming() && Aiming->IsComponentTickEnabled());
	Aiming->EndAim(true);
	TestFalse(TEXT("Releasing both modes stops presentation tick"), Aiming->IsComponentTickEnabled());
	Character->GetFirstPersonCameraComponent()->SetRelativeRotation(OriginalCameraRotation);
	Sight->Mode = EFE_WeaponSightMode::Scope2D;
	Aiming->BeginAim();
	Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Hidden Scope2D arms do not get visual recoil"), Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity));
	TestTrue(TEXT("Scope2D does not move arms to the 3D aim transform"), Arms->GetRelativeTransform().Equals(Weapon->Template.FirstPersonMeshTransform));
	TestFalse(TEXT("Scope2D hides arms"), Arms->IsVisible());
	TestFalse(TEXT("Scope2D hides the first-person weapon"), Equipment->GetEquippedFirstPersonWeaponMesh()->IsVisible());
	TestFalse(TEXT("Scope2D stops ticking after blend-in"), Aiming->IsComponentTickEnabled());
	Aiming->EndAim(true);
	TestTrue(TEXT("Scope cancellation restores visibility"), Arms->IsVisible());
	TestTrue(TEXT("Scope cancellation restores weapon visibility"), Equipment->GetEquippedFirstPersonWeaponMesh()->IsVisible());
	Sight->Mode = EFE_WeaponSightMode::IronSights;
	UFE_WeaponSightData* RedDot = NewObject<UFE_WeaponSightData>();
	RedDot->Mode = EFE_WeaponSightMode::RedDot;
	RedDot->Mesh = WeaponMeshAsset;
	RedDot->AttachSocketName = Sight->AimSocketName;
	RedDot->SocketOffset = FTransform(FVector(2, 3, 4));
	FFE_WeaponAttachmentInstance Part;
	Part.Definition = RedDot;
	TestTrue(TEXT("Mount a static sight"), Equipment->SetWeaponAttachments({Part}));
	TestTrue(TEXT("Mounted sight selects its own mesh"), Equipment->GetFirstPersonSightMesh() != Equipment->GetEquippedFirstPersonWeaponMesh());
	TestTrue(TEXT("Sight attaches to its configured weapon socket"), Equipment->GetFirstPersonSightMesh()->GetAttachSocketName() == RedDot->AttachSocketName);
	Aiming->BeginAim();
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Mounted optic socket is centered, not the weapon socket"), Equipment->GetFirstPersonSightMesh()->GetSocketTransform(RedDot->AimSocketName)
		.GetRelativeTransform(Character->GetFirstPersonCameraComponent()->GetComponentTransform()).GetLocation().Equals(FVector(RedDot->EyeReliefDistance, 0, 0), 0.001f));
	UFE_WeaponSightData* SkeletalSight = NewObject<UFE_WeaponSightData>();
	SkeletalSight->Mesh = WeaponMeshAsset;
	SkeletalSight->SkeletalMesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Combat/Character/PlayerCharacter/Mesh/SKM_HT_Arms.SKM_HT_Arms"));
	SkeletalSight->AimSocketName = TEXT("hand_r");
	SkeletalSight->AttachSocketName = Sight->AimSocketName;
	Part.Definition = SkeletalSight;
	TestTrue(TEXT("Mount a skeletal sight"), Equipment->SetWeaponAttachments({Part}));
	TestTrue(TEXT("Skeletal sight takes priority over static mesh"), Equipment->GetFirstPersonSightMesh()->IsA<USkeletalMeshComponent>());
	TestTrue(TEXT("Skeletal sight socket supports ADS"), Aiming->CanAim());
	UFallenEraAbilitySystemComponent* ASC = Character->GetFallenEraAbilitySystemComponent();
	TestTrue(TEXT("Real aim GA activates"), ASC->TryActivateAbilityByClass(UFE_WeaponAimAbility::StaticClass()));
	Aiming->TickComponent(0.3f, LEVELTICK_All, nullptr);
	const float AimedFOV = Character->GetFirstPersonCameraComponent()->FieldOfView;
	const FGameplayTagContainer AimTags(FallenEraCombatGameplayTags::Ability_Combat_Aim);
	ASC->CancelAbilities(&AimTags);
	TestFalse(TEXT("Ability cancellation ends ADS gameplay immediately"), Aiming->IsAiming());
	TestEqual(TEXT("Ability cancellation does not snap the camera FOV"), Character->GetFirstPersonCameraComponent()->FieldOfView, AimedFOV);
	Aiming->TickComponent(SkeletalSight->BlendOutTime * 0.5f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Cancelled camera returns through an intermediate FOV"),
		Character->GetFirstPersonCameraComponent()->FieldOfView > AimedFOV && Character->GetFirstPersonCameraComponent()->FieldOfView < OriginalFOV);
	Aiming->TickComponent(SkeletalSight->BlendOutTime, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Cancelled camera restores the original FOV"), Character->GetFirstPersonCameraComponent()->FieldOfView, OriginalFOV);
	Aiming->BeginAim();
	Aiming->TickComponent(0.2f, LEVELTICK_All, nullptr);
	Aiming->ApplyShotRecoil(VisualRecoil, 0.0f);
	Aiming->TickComponent(0.02f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Visual recoil is active before swapping"), Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity));
	const float PreSwapFOV = Character->GetFirstPersonCameraComponent()->FieldOfView;
	Equipment->EquipWeaponByItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick);
	TestFalse(TEXT("Swap cancels ADS"), Aiming->IsAiming());
	TestTrue(TEXT("Swap clears the previous weapon's visual kick"), Aiming->GetVisualRecoilTransform().Equals(FTransform::Identity));
	TestTrue(TEXT("Swap applies the new base, not the previous aim pose"), Arms->GetRelativeTransform().Equals(SecondWeapon->Template.FirstPersonMeshTransform));
	TestEqual(TEXT("Destroyed old optic does not snap the camera"), Character->GetFirstPersonCameraComponent()->FieldOfView, PreSwapFOV);
	Aiming->TickComponent(SkeletalSight->BlendOutTime * 0.5f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Old optic blends camera out after the new weapon is equipped"),
		Character->GetFirstPersonCameraComponent()->FieldOfView > PreSwapFOV && Character->GetFirstPersonCameraComponent()->FieldOfView < OriginalFOV);
	TestTrue(TEXT("Old camera blend cannot overwrite the new weapon pose"), Arms->GetRelativeTransform().Equals(SecondWeapon->Template.FirstPersonMeshTransform));
	TestTrue(TEXT("New weapon can aim before the old camera blend finishes"), Aiming->BeginAim());
	const float NewAimStartFOV = Character->GetFirstPersonCameraComponent()->FieldOfView;
	TestTrue(TEXT("Re-aim starts from the current camera, not a reset FOV"), NewAimStartFOV < OriginalFOV);
	Aiming->TickComponent(0.3f, LEVELTICK_All, nullptr);
	Aiming->EndAim();
	Aiming->TickComponent(0.3f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("New weapon release preserves the original unzoomed baseline"), Character->GetFirstPersonCameraComponent()->FieldOfView, OriginalFOV);
	Equipment->UnequipWeapon();
	TestTrue(TEXT("Unequip restores original character arms pose"), Arms->GetRelativeTransform().Equals(UnequippedTransform));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ChargeSwayTest, "FallenEra.Combat.Presentation.ChargeSway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ChargeSwayTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UClass* CharacterClass = LoadClass<AFallenEraCharacter>(nullptr,
		TEXT("/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter.BP_HT_PlayerCharacter_C"));
	if (!TestNotNull(TEXT("Player class"), CharacterClass))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	AFallenEraCharacter* Character = World->SpawnActor<AFallenEraCharacter>(CharacterClass);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Character);
	UFE_WeaponAimingComponent* Aiming = Character->FindComponentByClass<UFE_WeaponAimingComponent>();
	USkeletalMeshComponent* Arms = Character->GetFirstPersonMesh();
	UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
	const FTransform Base = Arms->GetRelativeTransform();
	const float FOV = Camera->FieldOfView;
	const FRotator ControlRotation = Controller->GetControlRotation();
	UObject* Source = NewObject<UFE_MeleeAttackData>(Aiming);
	UObject* OtherSource = NewObject<UFE_MeleeAttackData>(Aiming);
	FFE_WeaponAimSwaySettings Settings;
	Settings.SmoothingTime = 0.0f;
	Settings.MovementLocationAmplitude = FVector::ZeroVector;
	Settings.MovementRotationAmplitude = FRotator::ZeroRotator;
	Aiming->BeginChargeSway(Source, Settings);
	TestTrue(TEXT("Charge needs no optic or ADS support"), Aiming->IsComponentTickEnabled());
	TestFalse(TEXT("Charging is not ADS"), Aiming->IsAiming());
	Camera->AddRelativeRotation(FRotator(0.0f, 10.0f, 0.0f));
	Aiming->TickComponent(0.1f, LEVELTICK_All, nullptr);
	const FTransform FirstOffset = Arms->GetRelativeTransform();
	TestFalse(TEXT("Charge look input moves arms"), FirstOffset.Equals(Base, 0.0001f));
	TestFalse(TEXT("Charge shares the existing sway calculator"), Aiming->GetAimSwayTransform().Equals(FTransform::Identity));
	TestEqual(TEXT("Charge sway does not zoom camera"), Camera->FieldOfView, FOV);
	TestTrue(TEXT("Charge sway does not alter ballistic aim"), Controller->GetControlRotation().Equals(ControlRotation));
	Aiming->BeginChargeSway(Source, Settings);
	Camera->AddRelativeRotation(FRotator(0.0f, 10.0f, 0.0f));
	Aiming->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Held start cannot recapture and accumulate sway"), Arms->GetRelativeTransform().Equals(FirstOffset, 0.001f));
	Aiming->EndAim(true);
	TestTrue(TEXT("Ending absent ADS does not recapture a swayed baseline"), Arms->GetRelativeTransform().Equals(Base));
	TestTrue(TEXT("Ending absent ADS preserves ongoing charge"), Aiming->IsComponentTickEnabled());
	Aiming->EndChargeSway(OtherSource);
	TestTrue(TEXT("Another ability cannot stop the active charge sway"), Aiming->IsComponentTickEnabled());
	Aiming->EndChargeSway(Source);
	TestTrue(TEXT("Release restores the exact equipped baseline"), Arms->GetRelativeTransform().Equals(Base));
	TestTrue(TEXT("Release clears sway"), Aiming->GetAimSwayTransform().Equals(FTransform::Identity));
	TestFalse(TEXT("Release disables charge-only tick"), Aiming->IsComponentTickEnabled());
	Settings.bEnabled = false;
	Aiming->BeginChargeSway(Source, Settings);
	TestFalse(TEXT("Disabled setting has no charge tick"), Aiming->IsComponentTickEnabled());
	Settings.bEnabled = true;
	Settings.MovementLocationAmplitude = FVector(0.0f, 0.3f, 0.2f);
	Character->GetCharacterMovement()->Velocity = FVector(300.0f, 0.0f, 0.0f);
	Aiming->BeginChargeSway(Source, Settings);
	Aiming->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Movement also creates charge sway"), Arms->GetRelativeTransform().Equals(Base, 0.0001f));
	Aiming->BeginPlay();
	Aiming->EndPlay(EEndPlayReason::Destroyed);
	TestTrue(TEXT("Teardown restores baseline"), Arms->GetRelativeTransform().Equals(Base));
	TestFalse(TEXT("Teardown stops charge tick"), Aiming->IsComponentTickEnabled());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ArmsSightAlignmentTest, "FallenEra.Combat.Attachments.ArmsSightAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ArmsSightAlignmentTest::RunTest(const FString& Parameters)
{
	const FTransform SightInArms(FRotator(10, 70, 5), FVector(20, 2, 150), FVector(0.3f));
	const FTransform DesiredSight(FRotator(-15, 140, 0), FVector(12, 0, 0));
	const FVector ArmsScale(0.6f);
	const FTransform Aligned = UFE_WeaponAimingComponent::CalculateAlignedArmsTransform(SightInArms, DesiredSight, ArmsScale);
	const FTransform Result = SightInArms * Aligned;
	TestTrue(TEXT("Aim socket lands at camera center"), Result.GetLocation().Equals(DesiredSight.GetLocation(), 0.001f));
	TestTrue(TEXT("Socket forward/up axes align with camera"), Result.GetRotation().Equals(DesiredSight.GetRotation(), 0.001f));
	TestTrue(TEXT("Optic scale does not resize arms"), Aligned.GetScale3D().Equals(ArmsScale));
	return true;
}

#endif
