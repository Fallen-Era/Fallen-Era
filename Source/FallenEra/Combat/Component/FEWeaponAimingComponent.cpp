#include "Combat/Component/FEWeaponAimingComponent.h"

#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEPlayerMovementStatsComponent.h"
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
	bIsAiming = true;
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
		WeaponMesh = Equipment->GetEquippedFirstPersonWeaponMesh();
		ArmsMesh = IFE_CombatPresentation::FindFirstPersonMesh(GetOwner());
		OriginalArmsTransform = ArmsMesh->GetRelativeTransform();
		SightMesh = Equipment->GetFirstPersonSightMesh();
		OriginalFOV = Camera->FieldOfView;
		OriginalFirstPersonFOV = Camera->FirstPersonFieldOfView;
		bOriginalWeaponVisible = WeaponMesh->IsVisible();
		bOriginalArmsVisible = ArmsMesh.IsValid() && ArmsMesh->IsVisible();
		bCapturedPresentation = true;
		if (const UFE_RangedWeaponItemData* Weapon = Cast<UFE_RangedWeaponItemData>(Equipment->GetCurrentWeaponData()))
		{
			CachedSwaySettings = Weapon->AimSwaySettings;
		}
		ResetSway();
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
	SetComponentTickEnabled(true);
	if (Sight->BlendInTime <= 0.0f) { AimAlpha = 1.0f; UpdatePresentation(); }
	return true;
}

void UFE_WeaponAimingComponent::EndAim(bool bImmediate)
{
	const bool bWasAiming = bIsAiming;
	bIsAiming = false;
	if (bWasAiming && GetOwner()->HasAuthority()) { GetOwner()->ForceNetUpdate(); }
	if (bImmediate || !Sight || Sight->BlendOutTime <= 0.0f) { RestorePresentation(); }
	else if (bCapturedPresentation) { SetComponentTickEnabled(true); }
}

void UFE_WeaponAimingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!IsLocallyControlled() || !Sight || !Camera.IsValid() || !WeaponMesh.IsValid() || !ArmsMesh.IsValid() ||
		!ArmsMesh->GetAttachParent() || (Sight->Mode != EFE_WeaponSightMode::Scope2D &&
			(!SightMesh.IsValid() || !SightMesh->DoesSocketExist(Sight->AimSocketName))))
	{
		RestorePresentation();
		return;
	}
	const float Duration = bIsAiming ? Sight->BlendInTime : Sight->BlendOutTime;
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bIsAiming ? 1.0f : 0.0f, DeltaTime,
		Duration > 0.0f ? 1.0f / Duration : BIG_NUMBER);
	UpdateSway(DeltaTime);
	UpdatePresentation();
	if (!bIsAiming && AimAlpha <= 0.0f) { RestorePresentation(); }
	else if (bIsAiming && AimAlpha >= 1.0f && Sight->Mode == EFE_WeaponSightMode::Scope2D)
	{
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
	if (!CachedSwaySettings.bEnabled || Sight->Mode == EFE_WeaponSightMode::Scope2D)
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

void UFE_WeaponAimingComponent::UpdatePresentation()
{
	const float Alpha = FMath::SmoothStep(0.0f, 1.0f, AimAlpha);
	Camera->SetFieldOfView(FMath::Lerp(OriginalFOV, Sight->AimFieldOfView, Alpha));
	Camera->SetFirstPersonFieldOfView(FMath::Lerp(OriginalFirstPersonFOV, Sight->AimFieldOfView, Alpha));
	if (Sight->Mode == EFE_WeaponSightMode::Scope2D)
	{
		// Attachments inherit visibility when hiding the parent weapon.
		WeaponMesh->SetVisibility(bOriginalWeaponVisible && Alpha < 0.5f, true);
		ArmsMesh->SetVisibility(bOriginalArmsVisible && Alpha < 0.5f, false);
		if (ScopeWidget) { ScopeWidget->SetRenderOpacity(Alpha); }
	}
	else
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

void UFE_WeaponAimingComponent::RestorePresentation()
{
	if (bCapturedPresentation)
	{
		if (Camera.IsValid())
		{
			Camera->SetFieldOfView(OriginalFOV);
			Camera->SetFirstPersonFieldOfView(OriginalFirstPersonFOV);
		}
		if (WeaponMesh.IsValid())
		{
			WeaponMesh->SetVisibility(bOriginalWeaponVisible, true);
		}
		if (ArmsMesh.IsValid())
		{
			ArmsMesh->SetRelativeTransform(OriginalArmsTransform);
			ArmsMesh->SetVisibility(bOriginalArmsVisible, false);
			RemoveTickPrerequisiteComponent(ArmsMesh.Get());
		}
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(WeaponMesh.Get())) { RemoveTickPrerequisiteComponent(Mesh); }
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(SightMesh.Get())) { RemoveTickPrerequisiteComponent(Mesh); }
	}
	if (ScopeWidget) { ScopeWidget->RemoveFromParent(); ScopeWidget = nullptr; }
	bCapturedPresentation = false;
	bIsAiming = false;
	AimAlpha = 0.0f;
	ResetSway();
	Sight = nullptr;
	Camera.Reset();
	WeaponMesh.Reset();
	ArmsMesh.Reset();
	SightMesh.Reset();
	SetComponentTickEnabled(false);
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
	EndAim(true);
	Super::EndPlay(Reason);
}

void UFE_WeaponAimingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UFE_WeaponAimingComponent, bIsAiming, COND_SkipOwner);
}
