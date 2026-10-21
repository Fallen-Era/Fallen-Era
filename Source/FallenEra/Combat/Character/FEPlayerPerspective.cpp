#include "FallenEraCharacter.h"

#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"

UCameraComponent* AFallenEraCharacter::GetCombatCamera() const
{
	return IsCombatFirstPersonView() ? FirstPersonCameraComponent : ThirdPersonCameraComponent.Get();
}

void AFallenEraCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	const float TargetAlpha = IsCombatFirstPersonView() ? 1.0f : 0.0f;
	PerspectiveBlendAlpha = FMath::FInterpConstantTo(PerspectiveBlendAlpha, TargetAlpha,
		DeltaTime, PerspectiveBlendTime > 0.0f ? 1.0f / PerspectiveBlendTime : BIG_NUMBER);
	if (bFirstPersonMeshesVisible != ShouldShowFirstPersonPresentation()) { ApplyPerspectivePresentation(); }
	UpdatePerspectiveSpringArm();
	if (bFirstPersonMeshesVisible)
	{
		FirstPersonCameraComponent->GetCameraView(DeltaTime, OutResult);
		return;
	}
	ThirdPersonCameraComponent->GetCameraView(DeltaTime, OutResult);
	if (PerspectiveBlendAlpha <= 0.0f) { return; }
	// Keep the hidden ADS camera's control rotation current for socket alignment before the cut.
	FMinimalViewInfo FirstView;
	FirstPersonCameraComponent->GetCameraView(DeltaTime, FirstView);
	// Only approach from outside the body. The final gap is a camera cut, not a path through the mesh.
	const FVector ToFirstPerson = FirstView.Location - OutResult.Location;
	const float Distance = ToFirstPerson.Size();
	const float Clearance = GetCapsuleComponent()->GetScaledCapsuleRadius() + 20.0f;
	const float Travel = FMath::Min(FMath::Max(0.0f, AimCameraApproachDistance), FMath::Max(0.0f, Distance - Clearance));
	const float Weight = FMath::SmoothStep(0.0f, FMath::Clamp(AimCameraSwitchAlpha, 0.1f, 1.0f), PerspectiveBlendAlpha);
	OutResult.Location += ToFirstPerson.GetSafeNormal() * Travel * Weight;
	OutResult.FOV = FMath::Lerp(OutResult.FOV, FirstView.FOV, Weight);
}

bool AFallenEraCharacter::ShouldShowFirstPersonPresentation() const
{
	return IsCombatFirstPersonView() && (!IsLocallyControlled() ||
		PerspectiveBlendAlpha >= FMath::Clamp(AimCameraSwitchAlpha, 0.1f, 1.0f));
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
	PerspectiveBlendAlpha = IsCombatFirstPersonView() ? 1.0f : 0.0f;
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
	PerspectiveBlendAlpha = IsCombatFirstPersonView() ? 1.0f : 0.0f;
	ApplyPerspectivePresentation();
}

void AFallenEraCharacter::SetCombatAimView(bool bAiming, float BlendTime)
{
	if (bAimFirstPersonView == bAiming) { return; }
	bAimFirstPersonView = bAiming;
	PerspectiveBlendTime = FMath::Max(0.0f, BlendTime);
	if (!IsLocallyControlled() || PerspectiveBlendTime <= 0.0f)
	{
		PerspectiveBlendAlpha = IsCombatFirstPersonView() ? 1.0f : 0.0f;
	}
	// Do not use SetThirdPersonView here: it cancels the very ADS ability requesting this override.
	ApplyPerspectivePresentation();
}

void AFallenEraCharacter::UpdatePerspectiveSpringArm()
{
	const bool bNeedsThirdPersonCamera = !IsCombatFirstPersonView() ||
		(IsLocallyControlled() && !bFirstPersonMeshesVisible);
	const bool bShouldTick = bNeedsThirdPersonCamera && (HasAuthority() || IsLocallyControlled());
	const bool bWasTicking = ThirdPersonSpringArm->IsComponentTickEnabled();
	ThirdPersonSpringArm->SetComponentTickEnabled(bShouldTick);
	if (bShouldTick && !bWasTicking) { ThirdPersonSpringArm->TickComponent(0.0f, LEVELTICK_All, nullptr); }
}

void AFallenEraCharacter::ResetPerspectiveAim()
{
	if (UFallenEraAbilitySystemComponent* ASC = GetFallenEraAbilitySystemComponent())
	{
		const FGameplayTagContainer AimTags(FallenEraCombatGameplayTags::Ability_Combat_Aim);
		ASC->CancelAbilities(&AimTags);
	}
	if (WeaponAimingComponent) { WeaponAimingComponent->PrepareForEquipmentChange(); }
	if (CombatComponent) { CombatComponent->StopChargeCameraPresentation(); }
}

void AFallenEraCharacter::ApplyPerspectivePresentation()
{
	bFirstPersonMeshesVisible = ShouldShowFirstPersonPresentation();
	FirstPersonCameraComponent->SetActive(bFirstPersonMeshesVisible);
	ThirdPersonCameraComponent->SetActive(!bFirstPersonMeshesVisible);
	UpdatePerspectiveSpringArm();
	// Align hidden arms throughout ADS, but reveal them only at the late camera cut.
	// Leaving ADS restores the world meshes immediately so FP arms cannot float during the pull-back.
	FirstPersonMesh->SetOwnerNoSee(!bFirstPersonMeshesVisible);
	FirstPersonLegsMesh->SetOwnerNoSee(!bFirstPersonMeshesVisible);
	// WorldSpaceRepresentation forces owner-hiding inside the renderer, regardless of OwnerNoSee.
	GetMesh()->SetFirstPersonPrimitiveType(bFirstPersonMeshesVisible
		? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None);
	GetMesh()->SetOwnerNoSee(bFirstPersonMeshesVisible);
	if (EquipmentComponent) { EquipmentComponent->RefreshPerspectivePresentation(); }
	if (CombatComponent) { CombatComponent->RefreshPerspectivePresentation(); }
}
