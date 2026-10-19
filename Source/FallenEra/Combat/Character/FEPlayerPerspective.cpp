#include "FallenEraCharacter.h"

#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

UCameraComponent* AFallenEraCharacter::GetCombatCamera() const
{
	return bThirdPersonView ? ThirdPersonCameraComponent.Get() : FirstPersonCameraComponent;
}

void AFallenEraCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	// Select explicitly; inherited Blueprints can contain other active camera components.
	if (UCameraComponent* Camera = GetCombatCamera()) { Camera->GetCameraView(DeltaTime, OutResult); }
	else { Super::CalcCamera(DeltaTime, OutResult); }
}

void AFallenEraCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFallenEraCharacter, bThirdPersonView, COND_SkipOwner);
}

void AFallenEraCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	SetCombatInputEnabled(true);
	ApplyPerspectivePresentation();
}

void AFallenEraCharacter::TogglePerspective()
{
	SetThirdPersonView(!bThirdPersonView);
}

void AFallenEraCharacter::HandleCombatTogglePerspective(const FInputActionValue& Value)
{
	if (IsLocallyControlled() && Value.Get<bool>()) { TogglePerspective(); }
}

void AFallenEraCharacter::SetThirdPersonView(bool bEnabled)
{
	if ((!HasAuthority() && !IsLocallyControlled()) || bThirdPersonView == bEnabled) { return; }
	ResetPerspectiveAim();
	bThirdPersonView = bEnabled;
	ApplyPerspectivePresentation();
	if (HasAuthority()) { ForceNetUpdate(); }
	else { ServerSetThirdPersonView(bEnabled); }
}

bool AFallenEraCharacter::ServerSetThirdPersonView_Validate(bool bEnabled)
{
	return true; // The owning connection may select either supported view; no client transform is accepted.
}

void AFallenEraCharacter::ServerSetThirdPersonView_Implementation(bool bEnabled)
{
	SetThirdPersonView(bEnabled);
}

void AFallenEraCharacter::OnRep_ThirdPersonView()
{
	ResetPerspectiveAim();
	ApplyPerspectivePresentation();
}

void AFallenEraCharacter::ResetPerspectiveAim()
{
	if (UFallenEraAbilitySystemComponent* ASC = GetFallenEraAbilitySystemComponent())
	{
		const FGameplayTagContainer AimTags(FallenEraCombatGameplayTags::Ability_Combat_Aim);
		ASC->CancelAbilities(&AimTags);
	}
	if (WeaponAimingComponent) { WeaponAimingComponent->EndAim(true); }
	if (CombatComponent) { CombatComponent->StopChargeCameraPresentation(); }
}

void AFallenEraCharacter::ApplyPerspectivePresentation()
{
	const bool bFirstPerson = !bThirdPersonView;
	FirstPersonCameraComponent->SetActive(bFirstPerson);
	ThirdPersonCameraComponent->SetActive(!bFirstPerson);
	ThirdPersonSpringArm->SetComponentTickEnabled(bThirdPersonView && (HasAuthority() || IsLocallyControlled()));
	if (ThirdPersonSpringArm->IsComponentTickEnabled()) { ThirdPersonSpringArm->TickComponent(0.0f, LEVELTICK_All, nullptr); }
	FirstPersonMesh->SetOwnerNoSee(!bFirstPerson);
	FirstPersonLegsMesh->SetOwnerNoSee(!bFirstPerson);
	GetMesh()->SetOwnerNoSee(bFirstPerson);
	if (EquipmentComponent) { EquipmentComponent->RefreshPerspectivePresentation(); }
	// Reset FP-only sway after the new mode is visible to the presentation interface.
	if (WeaponAimingComponent) { WeaponAimingComponent->EndAim(true); }
	if (CombatComponent) { CombatComponent->RefreshPerspectivePresentation(); }
}
