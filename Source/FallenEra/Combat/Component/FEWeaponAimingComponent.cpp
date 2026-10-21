#include "Combat/Component/FEWeaponAimingComponent.h"

#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Combat/Component/FEPlayerMovementStatsComponent.h"
#include "Combat/Component/FEStaminaComponent.h"
#include "Combat/Interface/FECombatPresentation.h"
#include "Combat/Weapon/Attachment/FEWeaponSightData.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/UI/FEWeaponScopeWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Net/UnrealNetwork.h"

UFE_WeaponAimingComponent::UFE_WeaponAimingComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UFE_WeaponAimingComponent::IsLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

bool UFE_WeaponAimingComponent::CanAim() const
{
	const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>();
	if (!Equipment || !Equipment->AreWeaponAttachmentsReady() || !Equipment->GetActiveSightData()) { return false; }
	const UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	// Tag and owner-only ammo state can arrive independently. Both must permit ADS,
	// including direct component calls and BP abilities with overridden tag defaults.
	if (Equipment->GetAmmoState().bReloading || (ASC && (ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading) ||
		ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_SelectingAmmo)))) { return false; }
	if (!IsLocallyControlled()) { return true; }
	const UFE_WeaponSightData* Data = Equipment->GetActiveSightData();
	const UMeshComponent* Mesh = Equipment->GetFirstPersonSightMesh();
	const USkeletalMeshComponent* Arms = IFE_CombatPresentation::FindFirstPersonMesh(GetOwner());
	return Cast<UFE_RangedWeaponItemData>(Equipment->GetCurrentWeaponData()) &&
		IFE_CombatPresentation::FindCombatCamera(GetOwner()) && Equipment->GetEquippedFirstPersonWeaponMesh() &&
		Arms && Arms->GetAttachParent() &&
		(Data->Mode == EFE_WeaponSightMode::Scope2D || (Mesh && Mesh->DoesSocketExist(Data->AimSocketName)));
}

bool UFE_WeaponAimingComponent::BeginAim()
{
	if (!CanAim()) { return false; }
	// Repeated held input must not recapture the aimed transform.
	if (bIsAiming) { return true; }
	// ADS must capture the uneffected baseline, not the previous frame's charge sway.
	StopChargeSwayPresentation();
	const bool bRecapture = bCapturedPresentation && bDetachedWeaponPresentation;
	const float BaselineFOV = OriginalFOV;
	const float BaselineFirstPersonFOV = OriginalFirstPersonFOV;
	if (bRecapture)
	{
		if (ScopeWidget) { ScopeWidget->RemoveFromParent(); ScopeWidget = nullptr; }
		bCapturedPresentation = false;
	}
	bIsAiming = true;
	RefreshAimView();
	if (UFE_PlayerMovementStatsComponent* Movement = GetOwner()->FindComponentByClass<UFE_PlayerMovementStatsComponent>();
		Movement && Movement->IsSprinting())
	{
		Movement->SetSprinting(false);
	}
	if (GetOwner()->HasAuthority()) { GetOwner()->ForceNetUpdate(); }
	if (!IsLocallyControlled()) { return true; }
	if (!bCapturedPresentation)
	{
		UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>();
		Sight = Equipment->GetActiveSightData();
		Camera = IFE_CombatPresentation::FindCombatCamera(GetOwner());
		bFirstPersonPresentation = IFE_CombatPresentation::UsesFirstPersonView(GetOwner());
		WeaponMesh = bFirstPersonPresentation ? Equipment->GetEquippedFirstPersonWeaponMesh() : Equipment->GetEquippedWorldWeaponMesh();
		ArmsMesh = IFE_CombatPresentation::FindFirstPersonMesh(GetOwner());
		OriginalArmsTransform = ArmsMesh->GetRelativeTransform();
		SightMesh = Equipment->GetFirstPersonSightMesh();
		OriginalFOV = bRecapture ? BaselineFOV : Camera->FieldOfView;
		OriginalFirstPersonFOV = bRecapture ? BaselineFirstPersonFOV : Camera->FirstPersonFieldOfView;
		bOriginalWeaponVisible = WeaponMesh->IsVisible();
		bOriginalArmsVisible = ArmsMesh.IsValid() && ArmsMesh->IsVisible();
		bCapturedPresentation = true;
		bDetachedWeaponPresentation = false;
		if (const UFE_RangedWeaponItemData* Weapon = Cast<UFE_RangedWeaponItemData>(Equipment->GetCurrentWeaponData()))
		{
			CachedSwaySettings = Weapon->Template.AimSwaySettings;
		}
		ResetSway();
		ResetVisualRecoil();
		// Final socket transforms are measured after skeletal evaluation, including weapon/optic poses.
		AddTickPrerequisiteComponent(ArmsMesh.Get());
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(WeaponMesh.Get())) { AddTickPrerequisiteComponent(Mesh); }
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(SightMesh.Get())) { AddTickPrerequisiteComponent(Mesh); }
		if (Sight->Mode == EFE_WeaponSightMode::Scope2D)
		{
			APlayerController* PC = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
			UClass* WidgetClass = Sight->ScopeWidgetClass.IsNull()
				? UFE_WeaponScopeWidget::StaticClass() : Sight->ScopeWidgetClass.Get();
			if (PC && PC->GetLocalPlayer() && WidgetClass)
			{
				ScopeWidget = CreateWidget<UFE_WeaponScopeWidget>(PC, WidgetClass);
				if (ScopeWidget)
				{
					ScopeWidget->SetSightData(Sight);
					ScopeWidget->AddToViewport(100);
				}
			}
		}
	}
	StartCameraBlend(Sight->BlendInTime);
	SetComponentTickEnabled(true);
	if (Sight->BlendInTime <= 0.0f) { AimAlpha = 1.0f; UpdatePresentation(); }
	return true;
}

void UFE_WeaponAimingComponent::EndAim(bool bImmediate)
{
	const bool bWasAiming = bIsAiming;
	bIsAiming = false;
	RefreshAimView();
	if (bWasAiming && GetOwner()->HasAuthority()) { GetOwner()->ForceNetUpdate(); }
	if (bWasAiming && Sight) { StartCameraBlend(Sight->BlendOutTime); }
	if (bImmediate || !Sight || Sight->BlendOutTime <= 0.0f) { RestorePresentation(); }
	else if (bCapturedPresentation) { SetComponentTickEnabled(true); }
}

void UFE_WeaponAimingComponent::StartCameraBlend(float Duration)
{
	if (!Camera.IsValid()) { return; }
	CameraBlendStartFOV = Camera->FieldOfView;
	CameraBlendStartFirstPersonFOV = Camera->FirstPersonFieldOfView;
	CameraBlendElapsed = 0.0f;
	CameraBlendDuration = FMath::Max(0.0f, Duration);
}

void UFE_WeaponAimingComponent::PrepareForEquipmentChange()
{
	EndAim();
	if (bCapturedPresentation && !bDetachedWeaponPresentation)
	{
		ReleaseMeshPresentation();
		bDetachedWeaponPresentation = true;
		ResetSway();
		ResetVisualRecoil();
	}
}

void UFE_WeaponAimingComponent::OnRep_IsAiming()
{
	RefreshAimView();
}

void UFE_WeaponAimingComponent::RefreshAimView()
{
	if (UFE_StaminaComponent* Stamina = GetOwner()->FindComponentByClass<UFE_StaminaComponent>()) { Stamina->RefreshActivity(); }
	if (IFE_CombatPresentation* Presentation = Cast<IFE_CombatPresentation>(GetOwner()))
	{
		const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>();
		const UFE_WeaponSightData* Settings = Sight ? Sight.Get() : (Equipment ? Equipment->GetActiveSightData() : nullptr);
		const float BlendTime = Settings ? (bIsAiming ? Settings->BlendInTime : Settings->BlendOutTime) : 0.15f;
		Presentation->SetCombatAimView(bIsAiming, BlendTime);
	}
}

void UFE_WeaponAimingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!bCapturedPresentation)
	{
		if (!IsLocallyControlled() || !ChargeSwaySource.IsValid() || !bChargeSwayCaptured ||
			!Camera.IsValid() || !ArmsMesh.IsValid() || !ArmsMesh->GetAttachParent())
		{
			StopChargeSwayPresentation();
			SetComponentTickEnabled(false);
			return;
		}
		UpdateSway(DeltaTime);
		const FTransform ParentWorld = ArmsMesh->GetAttachParent()->GetSocketTransform(ArmsMesh->GetAttachSocketName());
		const FTransform CameraWorld = Camera->GetComponentTransform();
		const FTransform BaseInCamera = (ChargeArmsTransform * ParentWorld).GetRelativeTransform(CameraWorld);
		const FTransform SwayWorld = BaseInCamera * GetAimSwayTransform() * CameraWorld;
		ArmsMesh->SetRelativeTransform(SwayWorld.GetRelativeTransform(ParentWorld));
		return;
	}
	if (!IsLocallyControlled() || !Sight || !Camera.IsValid())
	{
		RestorePresentation();
		return;
	}
	if (!bDetachedWeaponPresentation && (!WeaponMesh.IsValid() || !ArmsMesh.IsValid() ||
		!ArmsMesh->GetAttachParent() || (bFirstPersonPresentation && Sight->Mode != EFE_WeaponSightMode::Scope2D &&
			(!SightMesh.IsValid() || !SightMesh->DoesSocketExist(Sight->AimSocketName)))))
	{
		PrepareForEquipmentChange(); // Missing old meshes must not force a camera snap.
	}
	const float Duration = bIsAiming ? Sight->BlendInTime : Sight->BlendOutTime;
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bIsAiming ? 1.0f : 0.0f, DeltaTime,
		Duration > 0.0f ? 1.0f / Duration : BIG_NUMBER);
	UpdateSway(DeltaTime);
	UpdateVisualRecoil(DeltaTime);
	CameraBlendElapsed += DeltaTime;
	UpdatePresentation();
	if (!bIsAiming && AimAlpha <= 0.0f && CameraBlendElapsed >= CameraBlendDuration) { RestorePresentation(); }
	else if (bIsAiming && AimAlpha >= 1.0f && CameraBlendElapsed >= CameraBlendDuration && Sight->Mode == EFE_WeaponSightMode::Scope2D)
	{
		SetComponentTickEnabled(false);
	}
}

void UFE_WeaponAimingComponent::BeginChargeSway(UObject* Source, const FFE_WeaponAimSwaySettings& Settings)
{
	if (!IsLocallyControlled() || !IsValid(Source)) { return; }
	if (ChargeSwaySource.Get() == Source) { return; }
	StopChargeSwayPresentation();
	ChargeSwaySource = Settings.bEnabled ? Source : nullptr;
	ChargeSwaySettings = Settings;
	if (ChargeSwaySource.IsValid() && !bCapturedPresentation) { StartChargeSwayPresentation(); }
}

void UFE_WeaponAimingComponent::EndChargeSway(UObject* Source)
{
	if (ChargeSwaySource.Get() != Source) { return; }
	ChargeSwaySource.Reset();
	StopChargeSwayPresentation();
	if (!bCapturedPresentation) { SetComponentTickEnabled(false); }
}

void UFE_WeaponAimingComponent::StartChargeSwayPresentation()
{
	if (!IsLocallyControlled() || !IFE_CombatPresentation::UsesFirstPersonView(GetOwner())) { return; }
	Camera = IFE_CombatPresentation::FindCombatCamera(GetOwner());
	ArmsMesh = IFE_CombatPresentation::FindFirstPersonMesh(GetOwner());
	if (!Camera.IsValid() || !ArmsMesh.IsValid() || !ArmsMesh->GetAttachParent()) { return; }
	ChargeArmsTransform = ArmsMesh->GetRelativeTransform();
	CachedSwaySettings = ChargeSwaySettings;
	bChargeSwayCaptured = true;
	ResetSway();
	SetComponentTickEnabled(true);
}

void UFE_WeaponAimingComponent::StopChargeSwayPresentation()
{
	if (!bChargeSwayCaptured) { return; }
	if (ArmsMesh.IsValid()) { ArmsMesh->SetRelativeTransform(ChargeArmsTransform); }
	bChargeSwayCaptured = false;
	ResetSway();
	if (!bCapturedPresentation)
	{
		Camera.Reset();
		ArmsMesh.Reset();
		SetComponentTickEnabled(false);
	}
}

void UFE_WeaponAimingComponent::ResetSway()
{
	SwayLocation = SwayLocationVelocity = SwayRotation = SwayRotationVelocity = FVector::ZeroVector;
	MovementPhase = 0.0f;
	PreviousViewRotation = Camera.IsValid() ? Camera->GetComponentRotation() : FRotator::ZeroRotator;
}

void UFE_WeaponAimingComponent::UpdateSway(float DeltaTime)
{
	if (!CachedSwaySettings.bEnabled || (Sight && (!bFirstPersonPresentation || Sight->Mode == EFE_WeaponSightMode::Scope2D)))
	{
		ResetSway();
		return;
	}
	if (DeltaTime <= 0.0f) { return; }
	const FRotator ViewRotation = Camera->GetComponentRotation();
	const FRotator ViewDelta = (ViewRotation - PreviousViewRotation).GetNormalized();
	PreviousViewRotation = ViewRotation;
	const float MaxSpeed = FMath::Max(0.0f, CachedSwaySettings.MaxLookSpeed);
	const float PitchSpeed = FMath::Clamp(ViewDelta.Pitch / DeltaTime, -MaxSpeed, MaxSpeed);
	const float YawSpeed = FMath::Clamp(ViewDelta.Yaw / DeltaTime, -MaxSpeed, MaxSpeed);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;
	const float MaxMoveSpeed = Movement ? FMath::Max(1.0f, Movement->GetMaxSpeed()) : 300.0f;
	const float MoveAlpha = Pawn ? FMath::Clamp(Pawn->GetVelocity().Size2D() / MaxMoveSpeed, 0.0f, 1.0f) : 0.0f;
	MovementPhase = FMath::Fmod(MovementPhase + DeltaTime * 2.0f * PI *
		FMath::Max(0.0f, CachedSwaySettings.MovementFrequency) * MoveAlpha, 2.0f * PI);
	const float Side = FMath::Sin(MovementPhase);
	const float Vertical = FMath::Cos(2.0f * MovementPhase);
	const FVector MoveLocation = CachedSwaySettings.MovementLocationAmplitude * FVector(Side, Side, Vertical) * MoveAlpha;
	const FRotator& Amplitude = CachedSwaySettings.MovementRotationAmplitude;
	const FVector MoveRotation = FVector(Amplitude.Pitch * Vertical, Amplitude.Yaw * Side, Amplitude.Roll * Side) * MoveAlpha;
	const float Scale = FMath::Clamp(CachedSwaySettings.AimMultiplier, 0.0f, 1.0f);
	const FVector TargetLocation = (FVector(0.0f, -YawSpeed, PitchSpeed) *
		FMath::Max(0.0f, CachedSwaySettings.LookLocationStrength) + MoveLocation) * Scale;
	const FVector TargetRotation = (FVector(-PitchSpeed, -YawSpeed, 0.0f) *
		FMath::Max(0.0f, CachedSwaySettings.LookRotationStrength) + MoveRotation) * Scale;
	const float SmoothingTime = FMath::Max(0.0f, CachedSwaySettings.SmoothingTime);
	FMath::CriticallyDampedSmoothing(SwayLocation, SwayLocationVelocity, TargetLocation, FVector::ZeroVector, DeltaTime, SmoothingTime);
	FMath::CriticallyDampedSmoothing(SwayRotation, SwayRotationVelocity, TargetRotation, FVector::ZeroVector, DeltaTime, SmoothingTime);
}

FTransform UFE_WeaponAimingComponent::GetAimSwayTransform() const
{
	return FTransform(FRotator(SwayRotation.X, SwayRotation.Y, SwayRotation.Z), SwayLocation);
}

void UFE_WeaponAimingComponent::ApplyShotRecoil(const FFE_WeaponVisualRecoilSettings& Settings, float RecoilControl)
{
	if (!IsLocallyControlled() || !bCapturedPresentation || !bIsAiming || !bFirstPersonPresentation ||
		!Sight || Sight->Mode == EFE_WeaponSightMode::Scope2D || !Settings.bEnabled ||
		Settings.KickTranslation.ContainsNaN() || Settings.KickRotation.ContainsNaN() || Settings.PivotOffset.ContainsNaN() ||
		!FMath::IsFinite(Settings.RecoveryTime) || !FMath::IsFinite(Settings.MaxStack) || !FMath::IsFinite(RecoilControl))
	{
		return;
	}
	const float Strength = 1.0f - FMath::Clamp(RecoilControl, 0.0f, 1.0f);
	if (Strength <= KINDA_SMALL_NUMBER) { return; }
	CachedVisualRecoilSettings = Settings;
	CachedVisualRecoilSettings.RecoveryTime = FMath::Clamp(Settings.RecoveryTime, 0.03f, 2.0f);
	CachedVisualRecoilSettings.MaxStack = FMath::Clamp(Settings.MaxStack, 1.0f, 10.0f);
	// A critically damped impulse peaks at v / (omega * e). Add velocity, not pose,
	// so another shot cannot snap or restart an already moving weapon.
	const float PeakToVelocity = 6.0f * FMath::Exp(1.0f) / CachedVisualRecoilSettings.RecoveryTime;
	const FVector RotationKick(Settings.KickRotation.Pitch,
		FMath::FRandRange(-FMath::Abs(Settings.KickRotation.Yaw), FMath::Abs(Settings.KickRotation.Yaw)),
		FMath::FRandRange(-FMath::Abs(Settings.KickRotation.Roll), FMath::Abs(Settings.KickRotation.Roll)));
	const FVector LocationLimit = Settings.KickTranslation.GetAbs() * (PeakToVelocity * CachedVisualRecoilSettings.MaxStack);
	const FVector RotationLimit = FVector(Settings.KickRotation.Pitch, Settings.KickRotation.Yaw, Settings.KickRotation.Roll)
		.GetAbs() * (PeakToVelocity * CachedVisualRecoilSettings.MaxStack);
	VisualRecoilLocationVelocity = (VisualRecoilLocationVelocity + Settings.KickTranslation * (PeakToVelocity * Strength))
		.BoundToBox(-LocationLimit, LocationLimit);
	VisualRecoilRotationVelocity = (VisualRecoilRotationVelocity + RotationKick * (PeakToVelocity * Strength))
		.BoundToBox(-RotationLimit, RotationLimit);
}

void UFE_WeaponAimingComponent::UpdateVisualRecoil(float DeltaTime)
{
	if (DeltaTime <= 0.0f || !FMath::IsFinite(DeltaTime)) { return; }
	if (VisualRecoilLocation.IsZero() && VisualRecoilLocationVelocity.IsZero() &&
		VisualRecoilRotation.IsZero() && VisualRecoilRotationVelocity.IsZero()) { return; }
	const float SmoothingTime = CachedVisualRecoilSettings.RecoveryTime / 3.0f;
	FMath::CriticallyDampedSmoothing(VisualRecoilLocation, VisualRecoilLocationVelocity,
		FVector::ZeroVector, FVector::ZeroVector, DeltaTime, SmoothingTime);
	FMath::CriticallyDampedSmoothing(VisualRecoilRotation, VisualRecoilRotationVelocity,
		FVector::ZeroVector, FVector::ZeroVector, DeltaTime, SmoothingTime);
	const FVector LocationLimit = CachedVisualRecoilSettings.KickTranslation.GetAbs() * CachedVisualRecoilSettings.MaxStack;
	const FRotator& Kick = CachedVisualRecoilSettings.KickRotation;
	const FVector RotationLimit = FVector(Kick.Pitch, Kick.Yaw, Kick.Roll).GetAbs() * CachedVisualRecoilSettings.MaxStack;
	VisualRecoilLocation = VisualRecoilLocation.BoundToBox(-LocationLimit, LocationLimit);
	VisualRecoilRotation = VisualRecoilRotation.BoundToBox(-RotationLimit, RotationLimit);
	if (VisualRecoilLocation.IsNearlyZero(0.0001f) && VisualRecoilRotation.IsNearlyZero(0.0001f) &&
		VisualRecoilLocationVelocity.IsNearlyZero(0.001f) && VisualRecoilRotationVelocity.IsNearlyZero(0.001f))
	{
		ResetVisualRecoil();
	}
}

FTransform UFE_WeaponAimingComponent::GetVisualRecoilTransform() const
{
	const float Alpha = FMath::SmoothStep(0.0f, 1.0f, AimAlpha);
	const FQuat Rotation = FRotator(VisualRecoilRotation.X * Alpha,
		VisualRecoilRotation.Y * Alpha, VisualRecoilRotation.Z * Alpha).Quaternion();
	const FVector& Pivot = CachedVisualRecoilSettings.PivotOffset;
	return FTransform(Rotation, Pivot - Rotation.RotateVector(Pivot) + VisualRecoilLocation * Alpha);
}

void UFE_WeaponAimingComponent::ResetVisualRecoil()
{
	VisualRecoilLocation = VisualRecoilLocationVelocity = VisualRecoilRotation = VisualRecoilRotationVelocity = FVector::ZeroVector;
}

void UFE_WeaponAimingComponent::UpdatePresentation()
{
	const float Alpha = FMath::SmoothStep(0.0f, 1.0f, AimAlpha);
	const float CameraAlpha = CameraBlendDuration > 0.0f
		? FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(CameraBlendElapsed / CameraBlendDuration, 0.0f, 1.0f)) : 1.0f;
	Camera->SetFieldOfView(FMath::Lerp(CameraBlendStartFOV, bIsAiming ? Sight->AimFieldOfView : OriginalFOV, CameraAlpha));
	Camera->SetFirstPersonFieldOfView(FMath::Lerp(CameraBlendStartFirstPersonFOV, bIsAiming ? Sight->AimFieldOfView : OriginalFirstPersonFOV, CameraAlpha));
	if (ScopeWidget) { ScopeWidget->SetRenderOpacity(Alpha); }
	if (bDetachedWeaponPresentation) { AimAlphaChanged.Broadcast(Alpha); return; }
	if (Sight->Mode == EFE_WeaponSightMode::Scope2D)
	{
		// Attachments inherit visibility when hiding the parent weapon.
		WeaponMesh->SetVisibility(bOriginalWeaponVisible && Alpha < 0.5f, true);
		ArmsMesh->SetVisibility(bOriginalArmsVisible && Alpha < 0.5f, false);
		if (ScopeWidget) { ScopeWidget->SetRenderOpacity(Alpha); }
	}
	else if (bFirstPersonPresentation)
	{
		// Remove arbitrary animated socket drift, but explicitly preserve intentional camera-local sway.
		const FTransform SightInArms = SightMesh->GetSocketTransform(Sight->AimSocketName)
			.GetRelativeTransform(ArmsMesh->GetComponentTransform());
		const USceneComponent* Parent = ArmsMesh->GetAttachParent();
		const FTransform ParentWorld = Parent->GetSocketTransform(ArmsMesh->GetAttachSocketName());
		const FTransform SwayOffset = GetAimSwayTransform();
		const FTransform DesiredSightWorld = FTransform(SwayOffset.GetRotation(),
			FVector(FMath::Max(1.0f, Sight->EyeReliefDistance), 0.0f, 0.0f) + SwayOffset.GetLocation())
			* Camera->GetComponentTransform();
		const FTransform AimArmsTransform = CalculateAlignedArmsTransform(SightInArms,
			DesiredSightWorld.GetRelativeTransform(ParentWorld), OriginalArmsTransform.GetScale3D());
		FTransform Result;
		Result.Blend(OriginalArmsTransform, AimArmsTransform, Alpha);
		// Apply intentional shot motion AFTER optic alignment, in camera space. The next
		// socket measurement is arms-relative, so it cannot feed this offset back into ADS.
		if (!VisualRecoilLocation.IsZero() || !VisualRecoilRotation.IsZero())
		{
			const FTransform CameraWorld = Camera->GetComponentTransform();
			const FTransform ArmsInCamera = (Result * ParentWorld).GetRelativeTransform(CameraWorld);
			Result = (ArmsInCamera * GetVisualRecoilTransform() * CameraWorld).GetRelativeTransform(ParentWorld);
		}
		ArmsMesh->SetRelativeTransform(Result);
	}
	AimAlphaChanged.Broadcast(Alpha);
}

FTransform UFE_WeaponAimingComponent::CalculateAlignedArmsTransform(const FTransform& SightInArms,
	const FTransform& DesiredSightInParent, const FVector& ArmsScale)
{
	const FQuat Rotation = (DesiredSightInParent.GetRotation() * SightInArms.GetRotation().Inverse()).GetNormalized();
	const FVector Location = DesiredSightInParent.GetLocation() - Rotation.RotateVector(SightInArms.GetLocation() * ArmsScale);
	return FTransform(Rotation, Location, ArmsScale);
}

void UFE_WeaponAimingComponent::ReleaseMeshPresentation()
{
	if (WeaponMesh.IsValid()) { WeaponMesh->SetVisibility(bOriginalWeaponVisible, true); }
	if (ArmsMesh.IsValid())
	{
		ArmsMesh->SetRelativeTransform(OriginalArmsTransform);
		ArmsMesh->SetVisibility(bOriginalArmsVisible, false);
		RemoveTickPrerequisiteComponent(ArmsMesh.Get());
	}
	if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(WeaponMesh.Get())) { RemoveTickPrerequisiteComponent(Mesh); }
	if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(SightMesh.Get())) { RemoveTickPrerequisiteComponent(Mesh); }
	WeaponMesh.Reset();
	ArmsMesh.Reset();
	SightMesh.Reset();
}

void UFE_WeaponAimingComponent::RestorePresentation()
{
	// EndAim may be called while only charging (for example during equipment cancellation).
	StopChargeSwayPresentation();
	if (bCapturedPresentation)
	{
		if (Camera.IsValid())
		{
			Camera->SetFieldOfView(OriginalFOV);
			Camera->SetFirstPersonFieldOfView(OriginalFirstPersonFOV);
		}
		ReleaseMeshPresentation();
	}
	if (ScopeWidget) { ScopeWidget->RemoveFromParent(); ScopeWidget = nullptr; }
	bCapturedPresentation = false;
	bDetachedWeaponPresentation = false;
	bIsAiming = false;
	RefreshAimView();
	AimAlpha = 0.0f;
	ResetSway();
	ResetVisualRecoil();
	Sight = nullptr;
	Camera.Reset();
	WeaponMesh.Reset();
	ArmsMesh.Reset();
	SightMesh.Reset();
	SetComponentTickEnabled(false);
	if (ChargeSwaySource.IsValid()) { StartChargeSwayPresentation(); }
	AimAlphaChanged.Broadcast(0.0f);
}

float UFE_WeaponAimingComponent::GetAimAlpha() const
{
	return IsLocallyControlled() ? FMath::SmoothStep(0.0f, 1.0f, AimAlpha) : (bIsAiming ? 1.0f : 0.0f);
}

FTransform UFE_WeaponAimingComponent::GetWeaponSocketWorldTransform(FName SocketName) const
{
	const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>();
	const UMeshComponent* Mesh = Equipment ? Equipment->GetEquippedFirstPersonWeaponMesh() : nullptr;
	return Mesh ? Mesh->GetSocketTransform(SocketName) : FTransform::Identity;
}

void UFE_WeaponAimingComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	EndChargeSway(ChargeSwaySource.Get());
	EndAim(true);
	Super::EndPlay(Reason);
}

void UFE_WeaponAimingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UFE_WeaponAimingComponent, bIsAiming, COND_SkipOwner);
}
