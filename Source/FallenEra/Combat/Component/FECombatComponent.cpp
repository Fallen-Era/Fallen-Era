#include "Combat/Component/FECombatComponent.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Animation/FEBowAnimInstance.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Combat/Interface/FECombatPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/FECombatTeams.h"
#include "Combat/Component/FECharacterStatusComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/Projectile/FECombatProjectile.h"
#include "Combat/ObjectPool/FEProjectilePoolSubsystem.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UFE_CombatComponent::UFE_CombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
}

void UFE_CombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CameraTransitionTimer);
		World->GetTimerManager().ClearTimer(RecoilRecoveryTimer);
	}
	if (UCameraComponent* Camera = ChargeCamera.Get(); Camera && bOriginalCameraTransformCached)
	{
		Camera->SetRelativeTransform(OriginalCameraRelativeTransform);
	}
	ChargeCamera.Reset();
	bOriginalCameraTransformCached = false;
	Super::EndPlay(EndPlayReason);
}

void UFE_CombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFE_CombatComponent, bDeathCollisionDisabled);
}

UAbilitySystemComponent* UFE_CombatComponent::FindAbilitySystemComponent(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Actor))
	{
		return AbilitySystemInterface->GetAbilitySystemComponent();
	}

	return Actor->FindComponentByClass<UAbilitySystemComponent>();
}

void UFE_CombatComponent::PlayAttackMontage(UAnimMontage* Montage, bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Montage)
	{
		return;
	}
	if (Pawn->HasAuthority())
	{
		MulticastPlayAttackMontage(Montage, bPredictedByOwner);
	}
	else if (bPredictedByOwner && Pawn->IsLocallyControlled())
	{
		PlayAttackMontageLocal(Montage);
	}
}

void UFE_CombatComponent::MulticastPlayAttackMontage_Implementation(UAnimMontage* Montage, bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->HasAuthority() && Pawn->IsLocallyControlled() && bPredictedByOwner)
	{
		return;
	}
	PlayAttackMontageLocal(Montage);
}

void UFE_CombatComponent::PlayAttackMontageLocal(UAnimMontage* Montage)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Montage)
	{
		return;
	}
	ActiveAttackMontage = Montage;
	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			Anim->Montage_Play(Montage);
		}
	}
	// if (Character->IsLocallyControlled())
	// {
	// 	USkeletalMeshComponent* FirstPersonMesh = IFE_CombatPresentation::FindFirstPersonMesh(Character);
	// 	if (FirstPersonMesh && FirstPersonMesh != Character->GetMesh())
	// 	{
	// 		if (UAnimInstance* Anim = FirstPersonMesh->GetAnimInstance())
	// 		{
	// 			Anim->Montage_Play(Montage);
	// 		}
	// 	}
	// }
}

void UFE_CombatComponent::PlayWeaponMeshMontage(UAnimMontage* Montage, bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Montage)
	{
		return;
	}
	if (Pawn->HasAuthority())
	{
		MulticastPlayWeaponMeshMontage(Montage, bPredictedByOwner);
	}
	else if (bPredictedByOwner && Pawn->IsLocallyControlled())
	{
		MulticastPlayWeaponMeshMontage_Implementation(Montage, false);
	}
}

void UFE_CombatComponent::MulticastPlayWeaponMeshMontage_Implementation(
	UAnimMontage* Montage,
	bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Montage || (!Pawn->HasAuthority() && Pawn->IsLocallyControlled() && bPredictedByOwner))
	{
		return;
	}

	ActiveWeaponMeshMontage = Montage;
	const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>();
	auto PlayOnMesh = [Montage](UMeshComponent* Mesh)
	{
		if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Mesh))
		{
			if (UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(Montage);
			}
		}
	};
	if (Equipment)
	{
		PlayOnMesh(Equipment->GetEquippedWorldWeaponMesh());
		PlayOnMesh(Equipment->GetEquippedFirstPersonWeaponMesh());
	}
}

void UFE_CombatComponent::SetBowChargeAlpha(float NewChargeAlpha)
{
	const float ClampedAlpha = FMath::Clamp(NewChargeAlpha, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(BowChargeAlpha, ClampedAlpha))
	{
		return;
	}
	BowChargeAlpha = ClampedAlpha;
	BowChargeChangedDelegate.Broadcast(BowChargeAlpha);
}

void UFE_CombatComponent::SetBowVisualAlpha(float NewVisualAlpha)
{
	const float ClampedAlpha = FMath::Clamp(NewVisualAlpha, 0.0f, 1.0f);
	const UFE_EquipmentComponent* Equipment = GetOwner()
		? GetOwner()->FindComponentByClass<UFE_EquipmentComponent>()
		: nullptr;
	auto ApplyToMesh = [ClampedAlpha](UMeshComponent* Mesh)
	{
		if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Mesh))
		{
			if (UFEBowAnimInstance* BowAnim = Cast<UFEBowAnimInstance>(SkeletalMesh->GetAnimInstance()))
			{
				BowAnim->SetChargeAlpha(ClampedAlpha);
			}
		}
	};
	if (Equipment)
	{
		ApplyToMesh(Equipment->GetEquippedWorldWeaponMesh());
		ApplyToMesh(Equipment->GetEquippedFirstPersonWeaponMesh());
	}
}

void UFE_CombatComponent::ApplyLocalWeaponRecoil(
	const FFE_RecoilSettings& RecoilSettings,
	float RecoilControl)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Pawn->IsLocallyControlled() || !PlayerController || !World)
	{
		return;
	}

	const float RecoilScale = 1.0f - FMath::Clamp(RecoilControl, 0.0f, 1.0f);
	if (RecoilScale <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float MinPitch = FMath::Min(RecoilSettings.MinPitch, RecoilSettings.MaxPitch);
	const float MaxPitch = FMath::Max(RecoilSettings.MinPitch, RecoilSettings.MaxPitch);
	const float MinYaw = FMath::Min(RecoilSettings.MinYaw, RecoilSettings.MaxYaw);
	const float MaxYaw = FMath::Max(RecoilSettings.MinYaw, RecoilSettings.MaxYaw);
	const FRotator Kick(
		FMath::FRandRange(MinPitch, MaxPitch) * RecoilScale,
		FMath::FRandRange(MinYaw, MaxYaw) * RecoilScale,
		0.0f);
	if (Kick.IsNearlyZero())
	{
		return;
	}

	PlayerController->SetControlRotation((PlayerController->GetControlRotation() + Kick).GetNormalized());
	PendingRecoilRecovery += FRotator(-Kick.Pitch, -Kick.Yaw, 0.0);

	const float RecoveryDuration = FMath::Max(0.01f, RecoilSettings.RecoveryDuration);
	RecoilPitchRecoverySpeed = FMath::Max(
		RecoilPitchRecoverySpeed,
		FMath::Abs(PendingRecoilRecovery.Pitch) / RecoveryDuration);
	RecoilYawRecoverySpeed = FMath::Max(
		RecoilYawRecoverySpeed,
		FMath::Abs(PendingRecoilRecovery.Yaw) / RecoveryDuration);
	LastRecoilRecoveryTime = World->GetTimeSeconds();
	if (!World->GetTimerManager().IsTimerActive(RecoilRecoveryTimer))
	{
		World->GetTimerManager().SetTimer(
			RecoilRecoveryTimer,
			this,
			&UFE_CombatComponent::HandleRecoilRecovery,
			1.0f / 60.0f,
			true);
	}
}

void UFE_CombatComponent::HandleRecoilRecovery()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Pawn->IsLocallyControlled() || !PlayerController || !World)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(RecoilRecoveryTimer);
		}
		PendingRecoilRecovery = FRotator::ZeroRotator;
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const float DeltaTime = FMath::Max(0.0f, CurrentTime - LastRecoilRecoveryTime);
	LastRecoilRecoveryTime = CurrentTime;
	const auto ConsumeAxis = [DeltaTime](double& Remaining, double Speed)
	{
		const double Step = FMath::Min(FMath::Abs(Remaining), Speed * DeltaTime);
		const double Applied = FMath::Sign(Remaining) * Step;
		Remaining -= Applied;
		return Applied;
	};

	const double PitchStep = ConsumeAxis(PendingRecoilRecovery.Pitch, RecoilPitchRecoverySpeed);
	const double YawStep = ConsumeAxis(PendingRecoilRecovery.Yaw, RecoilYawRecoverySpeed);
	PlayerController->SetControlRotation(
		(PlayerController->GetControlRotation() + FRotator(PitchStep, YawStep, 0.0f)).GetNormalized());

	if (PendingRecoilRecovery.IsNearlyZero(0.001f))
	{
		PendingRecoilRecovery = FRotator::ZeroRotator;
		RecoilPitchRecoverySpeed = 0.0f;
		RecoilYawRecoverySpeed = 0.0f;
		World->GetTimerManager().ClearTimer(RecoilRecoveryTimer);
	}
}

void UFE_CombatComponent::StartChargeCameraPresentation(
	const FTransform& TargetRelativeTransform,
	float BlendInDuration,
	float BlendOutDuration)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	UCameraComponent* Camera = IFE_CombatPresentation::FindCombatCamera(Pawn);
	if (!Camera)
	{
		return;
	}
	ChargeCamera = Camera;
	if (!bOriginalCameraTransformCached)
	{
		OriginalCameraRelativeTransform = Camera->GetRelativeTransform();
		bOriginalCameraTransformCached = true;
	}
	CameraBlendOutDuration = FMath::Max(0.0f, BlendOutDuration);
	bChargeCameraPresentationActive = true;
	StartCameraTransition(TargetRelativeTransform, BlendInDuration, false);
}

void UFE_CombatComponent::StopChargeCameraPresentation()
{
	if (!bChargeCameraPresentationActive || !bOriginalCameraTransformCached)
	{
		return;
	}
	bChargeCameraPresentationActive = false;
	StartCameraTransition(OriginalCameraRelativeTransform, CameraBlendOutDuration, true);
}

void UFE_CombatComponent::StartCameraTransition(
	const FTransform& TargetTransform,
	float Duration,
	bool bReturning)
{
	UCameraComponent* Camera = ChargeCamera.Get();
	UWorld* World = GetWorld();
	if (!Camera || !World)
	{
		bChargeCameraPresentationActive = false;
		bOriginalCameraTransformCached = false;
		ChargeCamera.Reset();
		return;
	}

	World->GetTimerManager().ClearTimer(CameraTransitionTimer);
	CameraTransitionStartTransform = Camera->GetRelativeTransform();
	CameraTransitionTargetTransform = TargetTransform;
	CameraTransitionStartTime = World->GetTimeSeconds();
	CameraTransitionDuration = FMath::Max(0.0f, Duration);
	bCameraTransitionReturning = bReturning;
	if (CameraTransitionDuration <= KINDA_SMALL_NUMBER)
	{
		Camera->SetRelativeTransform(CameraTransitionTargetTransform);
		if (bCameraTransitionReturning)
		{
			bOriginalCameraTransformCached = false;
			ChargeCamera.Reset();
		}
		return;
	}

	World->GetTimerManager().SetTimer(
		CameraTransitionTimer,
		this,
		&UFE_CombatComponent::HandleCameraTransition,
		1.0f / 60.0f,
		true);
}

void UFE_CombatComponent::HandleCameraTransition()
{
	UCameraComponent* Camera = ChargeCamera.Get();
	UWorld* World = GetWorld();
	if (!Camera || !World)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(CameraTransitionTimer);
		}
		bOriginalCameraTransformCached = false;
		ChargeCamera.Reset();
		return;
	}

	const float LinearAlpha = FMath::Clamp(
		(World->GetTimeSeconds() - CameraTransitionStartTime) /
		FMath::Max(CameraTransitionDuration, KINDA_SMALL_NUMBER),
		0.0f,
		1.0f);
	FTransform BlendedTransform;
	BlendedTransform.Blend(
		CameraTransitionStartTransform,
		CameraTransitionTargetTransform,
		FMath::SmoothStep(0.0f, 1.0f, LinearAlpha));
	Camera->SetRelativeTransform(BlendedTransform);

	if (LinearAlpha >= 1.0f)
	{
		Camera->SetRelativeTransform(CameraTransitionTargetTransform);
		World->GetTimerManager().ClearTimer(CameraTransitionTimer);
		if (bCameraTransitionReturning)
		{
			bOriginalCameraTransformCached = false;
			ChargeCamera.Reset();
		}
	}
}

void UFE_CombatComponent::StopWeaponActionPresentation(bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	if (Pawn->HasAuthority())
	{
		MulticastStopWeaponActionPresentation(bPredictedByOwner);
	}
	else
	{
		StopWeaponActionPresentationLocal();
	}
}

void UFE_CombatComponent::MulticastStopWeaponActionPresentation_Implementation(bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->HasAuthority() && Pawn->IsLocallyControlled() && bPredictedByOwner)
	{
		return;
	}
	StopWeaponActionPresentationLocal();
}

void UFE_CombatComponent::StopWeaponActionPresentationLocal()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && ActiveAttackMontage)
	{
		auto StopMontage = [this](USkeletalMeshComponent* Mesh)
		{
			if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
			{
				AnimInstance->Montage_Stop(0.1f, ActiveAttackMontage);
			}
		};
		StopMontage(Character->GetMesh());
		USkeletalMeshComponent* FirstPersonMesh = IFE_CombatPresentation::FindFirstPersonMesh(Character);
		if (FirstPersonMesh != Character->GetMesh())
		{
			StopMontage(FirstPersonMesh);
		}
	}
	ActiveAttackMontage = nullptr;
	if (ActiveWeaponMeshMontage)
	{
		const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>();
		auto StopWeaponMontage = [this](UMeshComponent* Mesh)
		{
			if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Mesh))
			{
				if (UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance())
				{
					AnimInstance->Montage_Stop(0.1f, ActiveWeaponMeshMontage);
				}
			}
		};
		if (Equipment)
		{
			StopWeaponMontage(Equipment->GetEquippedWorldWeaponMesh());
			StopWeaponMontage(Equipment->GetEquippedFirstPersonWeaponMesh());
		}
	}
	ActiveWeaponMeshMontage = nullptr;
	SetBowChargeAlpha(0.0f);
	SetBowVisualAlpha(0.0f);
	StopChargeProjectilePresentationLocal();
	StopChargeCameraPresentation();
}

void UFE_CombatComponent::PlayWeaponAttackEffects(
	USoundBase* AttackSound,
	float SoundVolume,
	float SoundPitch,
	float NoiseLoudness,
	float NoiseMaxRange,
	UNiagaraSystem* MuzzleSystem,
	UParticleSystem* MuzzleParticleSystem,
	FName MuzzleSocketName,
	FVector MuzzleScale,
	bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	const bool bHasPresentation = AttackSound || MuzzleSystem || MuzzleParticleSystem;
	if (Pawn->HasAuthority())
	{
		if (AttackSound && NoiseLoudness > 0.0f)
		{
			ReportCombatNoise(NoiseLoudness, NoiseMaxRange, TEXT("WeaponAttack"));
		}
		if (bHasPresentation)
		{
			MulticastPlayWeaponAttackEffects(
				AttackSound, SoundVolume, SoundPitch, MuzzleSystem, MuzzleParticleSystem,
				MuzzleSocketName, MuzzleScale, bPredictedByOwner);
		}
	}
	else if (bHasPresentation && bPredictedByOwner && Pawn->IsLocallyControlled())
	{
		PlayWeaponAttackEffectsLocal(
			AttackSound, SoundVolume, SoundPitch, MuzzleSystem, MuzzleParticleSystem,
			MuzzleSocketName, MuzzleScale);
	}
}

void UFE_CombatComponent::ReportCombatNoise(float Loudness, float MaxRange, FName NoiseTag)
{
	AActor* NoiseInstigator = GetOwner();
	if (!NoiseInstigator || !NoiseInstigator->HasAuthority() || Loudness <= 0.0f)
	{
		return;
	}

	UAISense_Hearing::ReportNoiseEvent(
		NoiseInstigator,
		NoiseInstigator->GetActorLocation(),
		Loudness,
		NoiseInstigator,
		FMath::Max(0.0f, MaxRange),
		NoiseTag);
}

void UFE_CombatComponent::MulticastPlayWeaponAttackEffects_Implementation(
	USoundBase* AttackSound,
	float SoundVolume,
	float SoundPitch,
	UNiagaraSystem* MuzzleSystem,
	UParticleSystem* MuzzleParticleSystem,
	FName MuzzleSocketName,
	FVector MuzzleScale,
	bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->HasAuthority() && Pawn->IsLocallyControlled() && bPredictedByOwner)
	{
		return;
	}

	PlayWeaponAttackEffectsLocal(
		AttackSound, SoundVolume, SoundPitch, MuzzleSystem, MuzzleParticleSystem,
		MuzzleSocketName, MuzzleScale);
}

void UFE_CombatComponent::PlayWeaponAttackEffectsLocal(
	USoundBase* AttackSound,
	float SoundVolume,
	float SoundPitch,
	UNiagaraSystem* MuzzleSystem,
	UParticleSystem* MuzzleParticleSystem,
	FName MuzzleSocketName,
	const FVector& MuzzleScale)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UMeshComponent* WeaponMesh = nullptr;
	if (const UFE_EquipmentComponent* Equipment = Character->FindComponentByClass<UFE_EquipmentComponent>())
	{
		WeaponMesh = Character->IsLocallyControlled()
			? Equipment->GetEquippedFirstPersonWeaponMesh()
			: Equipment->GetEquippedWorldWeaponMesh();
		if (!WeaponMesh)
		{
			WeaponMesh = Equipment->GetEquippedWorldWeaponMesh();
		}
	}

	FTransform EffectTransform = WeaponMesh ? WeaponMesh->GetComponentTransform() : Character->GetActorTransform();
	if (WeaponMesh && !MuzzleSocketName.IsNone() && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		EffectTransform = WeaponMesh->GetSocketTransform(MuzzleSocketName, RTS_World);
	}

	if (AttackSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			Character,
			AttackSound,
			EffectTransform.GetLocation(),
			FMath::Max(0.0f, SoundVolume),
			FMath::Max(0.0f, SoundPitch));
	}

	if (MuzzleSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Character,
			MuzzleSystem,
			EffectTransform.GetLocation(),
			EffectTransform.Rotator(),
			MuzzleScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
	else if (MuzzleParticleSystem)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			Character,
			MuzzleParticleSystem,
			EffectTransform.GetLocation(),
			EffectTransform.Rotator(),
			MuzzleScale,
			true,
			EPSCPoolMethod::AutoRelease,
			true);
	}
}

void UFE_CombatComponent::StartChargeProjectilePresentation(
	TSubclassOf<AFE_CombatProjectile> ProjectileClass,
	EFE_ChargedProjectileAttachmentTarget AttachmentTarget,
	FName AttachSocketName,
	const FTransform& AttachOffset,
	bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !ProjectileClass)
	{
		return;
	}
	if (Pawn->HasAuthority())
	{
		MulticastStartChargeProjectilePresentation(
			ProjectileClass, AttachmentTarget, AttachSocketName, AttachOffset, bPredictedByOwner);
	}
	else if (bPredictedByOwner && Pawn->IsLocallyControlled())
	{
		StartChargeProjectilePresentationLocal(
			ProjectileClass, AttachmentTarget, AttachSocketName, AttachOffset);
	}
}

void UFE_CombatComponent::StopChargeProjectilePresentation(bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	if (Pawn->HasAuthority())
	{
		MulticastStopChargeProjectilePresentation(bPredictedByOwner);
	}
	else if (bPredictedByOwner && Pawn->IsLocallyControlled())
	{
		StopChargeProjectilePresentationLocal();
	}
}

void UFE_CombatComponent::MulticastStartChargeProjectilePresentation_Implementation(
	TSubclassOf<AFE_CombatProjectile> ProjectileClass,
	EFE_ChargedProjectileAttachmentTarget AttachmentTarget,
	FName AttachSocketName,
	FTransform AttachOffset,
	bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->HasAuthority() && Pawn->IsLocallyControlled() && bPredictedByOwner)
	{
		return;
	}
	StartChargeProjectilePresentationLocal(
		ProjectileClass, AttachmentTarget, AttachSocketName, AttachOffset);
}

void UFE_CombatComponent::MulticastStopChargeProjectilePresentation_Implementation(bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->HasAuthority() && Pawn->IsLocallyControlled() && bPredictedByOwner)
	{
		return;
	}
	StopChargeProjectilePresentationLocal();
}

void UFE_CombatComponent::StartChargeProjectilePresentationLocal(
	TSubclassOf<AFE_CombatProjectile> ProjectileClass,
	EFE_ChargedProjectileAttachmentTarget AttachmentTarget,
	FName AttachSocketName,
	const FTransform& AttachOffset)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !ProjectileClass || !Character->GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	StopChargeProjectilePresentationLocal();
	const FTransform PreviewTransform(Character->GetActorRotation(), Character->GetActorLocation());
	UFE_ProjectilePoolSubsystem* ProjectilePool =
		Character->GetWorld()->GetSubsystem<UFE_ProjectilePoolSubsystem>();
	ChargeProjectilePreview = ProjectilePool
		? ProjectilePool->AcquireProjectile(
			ProjectileClass,
			PreviewTransform,
			Character,
			Character,
			true)
		: nullptr;
	if (!ChargeProjectilePreview)
	{
		return;
	}

	ChargeProjectilePreview->ConfigureAsLocalPreview();

	USceneComponent* AttachParent = nullptr;
	if (AttachmentTarget == EFE_ChargedProjectileAttachmentTarget::WeaponMesh)
	{
		if (const UFE_EquipmentComponent* Equipment = Character->FindComponentByClass<UFE_EquipmentComponent>())
		{
			AttachParent = Character->IsLocallyControlled() && Equipment->GetEquippedFirstPersonWeaponMesh()
				? static_cast<USceneComponent*>(Equipment->GetEquippedFirstPersonWeaponMesh())
				: static_cast<USceneComponent*>(Equipment->GetEquippedWorldWeaponMesh());
		}
	}
	else
	{
		AttachParent = Character->IsLocallyControlled()
			? IFE_CombatPresentation::FindFirstPersonMesh(Character)
			: nullptr;
		if (!AttachParent)
		{
			AttachParent = Character->GetMesh();
		}
	}

	if (AttachParent)
	{
		ChargeProjectilePreview->AttachToComponent(
			AttachParent,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			AttachSocketName);
		ChargeProjectilePreview->SetActorRelativeTransform(AttachOffset);
	}
}

void UFE_CombatComponent::StopChargeProjectilePresentationLocal()
{
	if (ChargeProjectilePreview)
	{
		ChargeProjectilePreview->ReturnToPool();
		ChargeProjectilePreview = nullptr;
	}
}

bool UFE_CombatComponent::ApplyDamage(AActor* TargetActor)
{
	const TSubclassOf<UGameplayEffect> EffectClass = DamageEffectClass.IsNull()
		? UFE_DamageGameplayEffect::StaticClass()
		: DamageEffectClass.LoadSynchronous();

	return ApplyDamageInternal(TargetActor, EffectClass);
}

bool UFE_CombatComponent::ApplyDamageFromHit(
	AActor* TargetActor,
	const FHitResult& HitResult,
	const UFE_WeaponAttackData* AttackData)
{
	const TSubclassOf<UGameplayEffect> EffectClass = DamageEffectClass.IsNull()
		? UFE_DamageGameplayEffect::StaticClass()
		: DamageEffectClass.LoadSynchronous();

	return ApplyDamageInternal(TargetActor, EffectClass, &HitResult, AttackData);
}

bool UFE_CombatComponent::ApplyDamageWithEffect(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClassOverride)
{
	return ApplyDamageInternal(TargetActor, DamageEffectClassOverride);
}

bool UFE_CombatComponent::ApplyDamageWithEffectFromHit(
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> DamageEffectClassOverride,
	const FHitResult& HitResult,
	const UFE_WeaponAttackData* AttackData)
{
	return ApplyDamageInternal(
		TargetActor,
		DamageEffectClassOverride,
		&HitResult,
		AttackData);
}

float UFE_CombatComponent::CalculateReceivedKnockback(float IncomingKnockback) const
{
	if (!FMath::IsFinite(IncomingKnockback))
	{
		return 0.0f;
	}
	UAbilitySystemComponent* ASC = FindAbilitySystemComponent(GetOwner());
	if (ASC && ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Immune_Knockback))
	{
		return 0.0f;
	}
	const float Resistance = ASC ? ASC->GetNumericAttribute(UFallenEraAttributeSet::GetKnockbackResistanceAttribute()) : 0.0f;
	return FMath::Max(IncomingKnockback - FMath::Max(Resistance, 0.0f), 0.0f);
}

void UFE_CombatComponent::ApplyDamageReaction(const AActor* DamageSource, const FFE_AttackReactionData& ReactionData)
{
	AActor* Victim = GetOwner();
	if (!Victim || !Victim->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* VictimAbilitySystem = FindAbilitySystemComponent(Victim);
	const bool bVictimDead = VictimAbilitySystem
		&& VictimAbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);

	// A lethal hit already triggered the death montage from the AttributeSet.
	// Do not overwrite it with a knockback or hit-reaction montage afterwards.
	if (!bVictimDead)
	{
		if (ACharacter* VictimCharacter = Cast<ACharacter>(Victim))
		{
			const float FinalKnockback = CalculateReceivedKnockback(ReactionData.KnockbackAmount);
			if (FinalKnockback > 0.0f && DamageSource && DamageSource != Victim)
			{
				FVector KnockbackDirection = Victim->GetActorLocation() - DamageSource->GetActorLocation();
				KnockbackDirection.Z = 0.0f;
				KnockbackDirection = KnockbackDirection.GetSafeNormal();
				if (KnockbackDirection.IsNearlyZero())
				{
					KnockbackDirection = Victim->GetActorForwardVector();
				}
				VictimCharacter->LaunchCharacter(KnockbackDirection * FinalKnockback, true, true);
			}

			if (!HitReactionMontages.IsEmpty())
			{
				const int32 MontageIndex = FMath::RandHelper(HitReactionMontages.Num());
				if (UAnimMontage* HitMontage = HitReactionMontages[MontageIndex])
				{
					MulticastPlayHitReaction(HitMontage);
				}
			}
		}
	}

	if (!bVictimDead && ReactionData.StunDuration > 0.0f && FMath::IsFinite(ReactionData.StunDuration))
	{
		if (VictimAbilitySystem)
		{
			if (!bReactionStunActive)
			{
				// Stop an ability that was already active before State.Stunned was added.
				// ActivationBlockedTags only prevents new activations.
				VictimAbilitySystem->CancelAllAbilities();
				if (ACharacter* VictimCharacter = Cast<ACharacter>(Victim))
				{
					if (UCharacterMovementComponent* MovementComponent = VictimCharacter->GetCharacterMovement())
					{
						MovementComponent->StopMovementImmediately();
					}
					if (AController* Controller = VictimCharacter->GetController())
					{
						Controller->StopMovement();
					}
				}

				VictimAbilitySystem->AddLooseGameplayTag(
					FallenEraGameplayTags::State_Stunned,
					1,
					EGameplayTagReplicationState::TagAndCountToAll);
				bReactionStunActive = true;
			}

			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(StunTimerHandle);
				World->GetTimerManager().SetTimer(
					StunTimerHandle,
					this,
					&UFE_CombatComponent::ClearStunState,
					ReactionData.StunDuration,
					false);
			}
		}
	}
}

void UFE_CombatComponent::HandleDeath()
{
	AActor* Victim = GetOwner();
	if (!Victim || !Victim->HasAuthority() || bDeathMontagePlayed)
	{
		return;
	}

	bDeathMontagePlayed = true;
	bDeathCollisionDisabled = true;
	ApplyDeathCollisionState();
	Victim->ForceNetUpdate();
	if (DeathMontage)
	{
		MulticastPlayDeathMontage(DeathMontage);
	}
}

void UFE_CombatComponent::OnRep_DeathCollisionDisabled()
{
	if (bDeathCollisionDisabled)
	{
		ApplyDeathCollisionState();
	}
}

void UFE_CombatComponent::ApplyDeathCollisionState()
{
	AActor* Victim = GetOwner();
	if (!Victim || !bDeathCollisionDisabled)
	{
		return;
	}

	Victim->SetActorEnableCollision(false);
	if (ACharacter* Character = Cast<ACharacter>(Victim))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
	}
}

void UFE_CombatComponent::MulticastPlayHitReaction_Implementation(UAnimMontage* HitMontage)
{
	PlayHitReactionMontage(HitMontage);
}

void UFE_CombatComponent::PlayHitReactionMontage(UAnimMontage* HitMontage)
{
	if (!HitMontage)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = FindAbilitySystemComponent(GetOwner());
		AbilitySystem && AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead))
	{
		return;
	}

	if (ACharacter* VictimCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = VictimCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(HitMontage);
			}
		}

		if (VictimCharacter->IsLocallyControlled())
		{
			if (USkeletalMeshComponent* FirstPersonMesh = IFE_CombatPresentation::FindFirstPersonMesh(VictimCharacter))
			{
				if (UAnimInstance* AnimInstance = FirstPersonMesh->GetAnimInstance())
				{
					AnimInstance->Montage_Play(HitMontage);
				}
			}
		}
	}
}

void UFE_CombatComponent::MulticastPlayDeathMontage_Implementation(UAnimMontage* Montage)
{
	PlayDeathMontage(Montage);
}

void UFE_CombatComponent::PlayDeathMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}

	if (ACharacter* VictimCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = VictimCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(0.05f);
				AnimInstance->Montage_Play(Montage);
			}
		}

		if (VictimCharacter->IsLocallyControlled())
		{
			if (USkeletalMeshComponent* FirstPersonMesh = IFE_CombatPresentation::FindFirstPersonMesh(VictimCharacter))
			{
				if (UAnimInstance* AnimInstance = FirstPersonMesh->GetAnimInstance())
				{
					AnimInstance->Montage_Stop(0.05f);
					AnimInstance->Montage_Play(Montage);
				}
			}
		}
	}
}

void UFE_CombatComponent::ClearStunState()
{
	if (AActor* Victim = GetOwner(); Victim && Victim->HasAuthority())
	{
		if (bReactionStunActive)
		{
			if (UAbilitySystemComponent* AbilitySystem = FindAbilitySystemComponent(Victim))
			{
				AbilitySystem->RemoveLooseGameplayTag(
					FallenEraGameplayTags::State_Stunned,
					1,
					EGameplayTagReplicationState::TagAndCountToAll);
			}
			bReactionStunActive = false;
		}
	}
}

bool UFE_CombatComponent::ApplyDamageInternal(
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> EffectClass,
	const FHitResult* HitResult,
	const UFE_WeaponAttackData* AttackData)
{
	AActor* SourceActor = GetOwner();
	if (!SourceActor || SourceActor == TargetActor ||
		(!TargetActor && !HitResult))
	{
		return false;
	}
	if (TargetActor && FECombatTeams::AreSameTeam(SourceActor, TargetActor))
	{
		return false;
	}

	// Damage is authoritative. Abilities may still predict their own effects separately when needed.
	if (!SourceActor->HasAuthority())
	{
		return false;
	}

	UAbilitySystemComponent* SourceAbilitySystem = FindAbilitySystemComponent(SourceActor);
	FFE_CombatDamageRequest DamageRequest;
	DamageRequest.SourceActor = SourceActor;
	DamageRequest.TargetActor = TargetActor;
	DamageRequest.AttackData = const_cast<UFE_WeaponAttackData*>(AttackData);
	DamageRequest.DamageEffectClass = EffectClass;
	DamageRequest.bHasHitResult = HitResult != nullptr;
	if (HitResult)
	{
		DamageRequest.HitResult = *HitResult;
	}
	if (SourceAbilitySystem && SourceAbilitySystem->HasAttributeSetForAttribute(
		UFallenEraAttributeSet::GetAttackPowerAttribute()))
	{
		DamageRequest.SourceAttackPower = SourceAbilitySystem->GetNumericAttribute(
			UFallenEraAttributeSet::GetAttackPowerAttribute());
	}

	FFE_CombatDamageResult DamageResult;
	if (TargetActor && TargetActor->GetClass()->ImplementsInterface(UFE_Damageable::StaticClass()))
	{
		DamageResult = IFE_Damageable::Execute_ReceiveCombatDamage(TargetActor, DamageRequest);
	}

	if (HitResult && AttackData && !DamageResult.bImpactCueHandled && SourceAbilitySystem)
	{
		FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(const_cast<UFE_WeaponAttackData*>(AttackData));
		EffectContext.AddInstigator(SourceActor, SourceActor);
		EffectContext.AddHitResult(*HitResult, true);
		ExecuteImpactCue(SourceAbilitySystem, SourceActor, EffectContext, *HitResult, AttackData);
	}
	return DamageResult.bDamageApplied;
}

FFE_CombatDamageResult UFE_CombatComponent::ApplyGameplayEffectDamage(
	const FFE_CombatDamageRequest& DamageRequest)
{
	FFE_CombatDamageResult Result;
	AActor* TargetActor = GetOwner();
	if (!TargetActor || !TargetActor->HasAuthority() ||
		DamageRequest.TargetActor != TargetActor || !DamageRequest.SourceActor ||
		!DamageRequest.DamageEffectClass ||
		FECombatTeams::AreSameTeam(DamageRequest.SourceActor, TargetActor))
	{
		return Result;
	}

	UAbilitySystemComponent* SourceAbilitySystem = FindAbilitySystemComponent(DamageRequest.SourceActor);
	UAbilitySystemComponent* TargetAbilitySystem = FindAbilitySystemComponent(TargetActor);
	if (!SourceAbilitySystem || !TargetAbilitySystem)
	{
		return Result;
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
	if (DamageRequest.AttackData)
	{
		// SourceObject lets the damage GE and its GameplayCue resolve weapon-specific presentation.
		EffectContext.AddSourceObject(DamageRequest.AttackData);
	}
	EffectContext.AddInstigator(DamageRequest.SourceActor, DamageRequest.SourceActor);
	if (DamageRequest.bHasHitResult)
	{
		EffectContext.AddHitResult(DamageRequest.HitResult, true);
	}

	const FGameplayEffectSpecHandle EffectSpec = SourceAbilitySystem->MakeOutgoingSpec(
		DamageRequest.DamageEffectClass, 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		return Result;
	}

	const float HealthBeforeDamage = TargetAbilitySystem->HasAttributeSetForAttribute(
		UFallenEraAttributeSet::GetHealthAttribute())
		? TargetAbilitySystem->GetNumericAttribute(UFallenEraAttributeSet::GetHealthAttribute())
		: 0.0f;
	Result.bHandled = true;
	const FActiveGameplayEffectHandle AppliedHandle =
		TargetAbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	Result.bDamageApplied = AppliedHandle.WasSuccessfullyApplied();
	Result.bImpactCueHandled = Result.bDamageApplied && DamageRequest.bHasHitResult;
	if (Result.bDamageApplied)
	{
		const float HealthAfterDamage = TargetAbilitySystem->HasAttributeSetForAttribute(
			UFallenEraAttributeSet::GetHealthAttribute())
			? TargetAbilitySystem->GetNumericAttribute(UFallenEraAttributeSet::GetHealthAttribute())
			: HealthBeforeDamage;
		const FVector HitLocation = DamageRequest.bHasHitResult
			? FVector(DamageRequest.HitResult.ImpactPoint)
			: TargetActor->GetActorLocation();
		UAISense_Damage::ReportDamageEvent(
			TargetActor,
			TargetActor,
			DamageRequest.SourceActor,
			FMath::Max(0.0f, HealthBeforeDamage - HealthAfterDamage),
			DamageRequest.SourceActor->GetActorLocation(),
			HitLocation,
			TEXT("CombatDamage"));

		if (HealthAfterDamage < HealthBeforeDamage)
		{
			if (UFE_CharacterStatusComponent* StatusComponent =
				TargetActor->FindComponentByClass<UFE_CharacterStatusComponent>())
			{
				StatusComponent->TryApplyConditionsFromDamageSource(DamageRequest.SourceActor);
			}
		}
	}
	if (Result.bDamageApplied && DamageRequest.AttackData)
	{
		ApplyDamageReaction(DamageRequest.SourceActor, DamageRequest.AttackData->AttackReactionData);
	}
	return Result;
}

void UFE_CombatComponent::ExecuteImpactCue(
	UAbilitySystemComponent* SourceAbilitySystem,
	AActor* SourceActor,
	const FGameplayEffectContextHandle& EffectContext,
	const FHitResult& HitResult,
	const UFE_WeaponAttackData* AttackData) const
{
	if (!SourceAbilitySystem || !SourceActor || !AttackData)
	{
		return;
	}

	FGameplayCueParameters CueParameters(EffectContext);
	CueParameters.Location = HitResult.ImpactPoint;
	CueParameters.Normal = HitResult.ImpactNormal;
	CueParameters.PhysicalMaterial = HitResult.PhysMaterial.Get();
	CueParameters.Instigator = SourceActor;
	CueParameters.EffectCauser = SourceActor;
	CueParameters.SourceObject = const_cast<UFE_WeaponAttackData*>(AttackData);
	SourceAbilitySystem->ExecuteGameplayCue(FallenEraCombatGameplayTags::GameplayCue_Impact, CueParameters);
}
