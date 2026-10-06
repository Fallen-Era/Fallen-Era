#include "Combat/Component/FECombatComponent.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Animation/FEBowAnimInstance.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/GameStateBase.h"
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

// Local presentation and its network reconstruction. Gameplay damage/lifetime stays in FECombatComponent.cpp.

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
	if (!Character || !Montage || bDeathCollisionDisabled)
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
			LocalPresentationUpdateInterval,
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
		LocalPresentationUpdateInterval,
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
	LocalChargeState = FFE_ChargePresentationState();
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
	const UFE_WeaponItemData* WeaponData, const UFE_WeaponAttackData* AttackData, bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !WeaponData || !AttackData) { return; }
	const int32 AttackIndex = WeaponData->AttackActions.IndexOfByPredicate(
		[AttackData](const TObjectPtr<UFE_WeaponAttackData>& Entry) { return Entry.Get() == AttackData; });
	if (AttackIndex == INDEX_NONE) { return; }
	if (Pawn->HasAuthority())
	{
		if (!WeaponData->AttackSound.IsNull() && WeaponData->AttackNoiseLoudness > 0.0f)
		{
			ReportCombatNoise(WeaponData->AttackNoiseLoudness, WeaponData->AttackNoiseMaxRange, TEXT("WeaponAttack"));
		}
		MulticastPlayWeaponAttackEffects(const_cast<UFE_WeaponItemData*>(WeaponData), AttackIndex, bPredictedByOwner);
	}
	else if (bPredictedByOwner && Pawn->IsLocallyControlled())
	{
		PlayWeaponDataEffectsLocal(WeaponData, AttackIndex);
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
	UFE_WeaponItemData* WeaponData, int32 AttackIndex, bool bPredictedByOwner)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->HasAuthority() && Pawn->IsLocallyControlled() && bPredictedByOwner) { return; }
	PlayWeaponDataEffectsLocal(WeaponData, AttackIndex);
}

void UFE_CombatComponent::PlayWeaponDataEffectsLocal(const UFE_WeaponItemData* WeaponData, int32 AttackIndex)
{
	if (!WeaponData || !WeaponData->AttackActions.IsValidIndex(AttackIndex)) { return; }
	const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>();
	if (!Equipment || Equipment->GetCurrentWeaponData() != WeaponData || bDeathCollisionDisabled) { return; }
	const UFE_WeaponAttackData* AttackData = WeaponData->AttackActions[AttackIndex];
	FName SocketName = NAME_None;
	if (const UFE_HitscanAttackData* Hitscan = Cast<UFE_HitscanAttackData>(AttackData)) { SocketName = Hitscan->MuzzleSocketName; }
	else if (const UFE_ProjectileAttackDataBase* Projectile = Cast<UFE_ProjectileAttackDataBase>(AttackData))
	{
		SocketName = Projectile->ProjectileSpawnSocketName;
	}
	const UFE_RangedWeaponItemData* Ranged = Cast<UFE_RangedWeaponItemData>(WeaponData);
	PlayWeaponAttackEffectsLocal(WeaponData->AttackSound.Get(), WeaponData->AttackSoundVolume, WeaponData->AttackSoundPitch,
		Ranged ? Ranged->MuzzleNiagaraSystem.Get() : nullptr, Ranged ? Ranged->MuzzleParticleSystem.Get() : nullptr,
		SocketName, Ranged ? Ranged->MuzzleNiagaraScale : FVector::OneVector);
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
	bool bPredictedByOwner,
	float FullChargeSeconds,
	UAnimMontage* ChargeMontage,
	bool bBow)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !ProjectileClass)
	{
		return;
	}
	FFE_ChargePresentationState State;
	State.StartServerTime = GetPresentationServerTime();
	State.FullChargeSeconds = FullChargeSeconds;
	State.bBow = bBow;
	State.ProjectileClass = ProjectileClass;
	State.Montage = ChargeMontage;
	State.AttachmentTarget = AttachmentTarget;
	State.SocketName = AttachSocketName;
	State.Offset = AttachOffset;
	if (Pawn->HasAuthority())
	{
		ChargeState = State;
		GetOwner()->ForceNetUpdate();
	}
	if (Pawn->IsLocallyControlled()) { LocalChargeState = State; }
	if (Pawn->HasAuthority() || (bPredictedByOwner && Pawn->IsLocallyControlled()))
	{
		StartChargeProjectilePresentationLocal(ProjectileClass, AttachmentTarget, AttachSocketName, AttachOffset);
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
		ChargeState = FFE_ChargePresentationState();
		GetOwner()->ForceNetUpdate();
	}
	LocalChargeState = FFE_ChargePresentationState();
	if (ChargeAssetLoadHandle) { ChargeAssetLoadHandle->CancelHandle(); ChargeAssetLoadHandle.Reset(); }
	if (Pawn->HasAuthority() || Pawn->IsLocallyControlled())
	{
		StopChargeProjectilePresentationLocal();
		SetBowChargeAlpha(0.0f);
	}
}

void UFE_CombatComponent::OnRep_ChargeState(const FFE_ChargePresentationState& PreviousState)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	// The predicting owner owns its local start/release; late server acknowledgements must not restart it.
	if (Pawn && Pawn->IsLocallyControlled()) { return; }
	if (ChargeAssetLoadHandle) { ChargeAssetLoadHandle->CancelHandle(); ChargeAssetLoadHandle.Reset(); }
	StopChargeProjectilePresentationLocal();
	if (ChargeState.StartServerTime < 0.0f || bDeathCollisionDisabled)
	{
		if (ActiveAttackMontage && ActiveAttackMontage == PreviousState.Montage.Get())
		{
			StopWeaponActionPresentationLocal();
		}
		SetBowVisualAlpha(0.0f);
		return;
	}
	TArray<FSoftObjectPath> Paths;
	if (!ChargeState.ProjectileClass.IsNull()) { Paths.Add(ChargeState.ProjectileClass.ToSoftObjectPath()); }
	if (!ChargeState.Montage.IsNull()) { Paths.Add(ChargeState.Montage.ToSoftObjectPath()); }
	ChargeAssetLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::RestoreChargePresentation));
}

void UFE_CombatComponent::HandleWeaponChanged(const UFE_WeaponItemData* WeaponData)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (WeaponData && Pawn && !Pawn->IsLocallyControlled()) { RestoreChargePresentation(); }
}

void UFE_CombatComponent::RestoreChargePresentation()
{
	if (ChargeState.StartServerTime < 0.0f || bDeathCollisionDisabled) { return; }
	StartChargeProjectilePresentationLocal(ChargeState.ProjectileClass.Get(), ChargeState.AttachmentTarget,
		ChargeState.SocketName, ChargeState.Offset);
	if (UAnimMontage* Montage = ChargeState.Montage.Get())
	{
		PlayAttackMontageLocal(Montage);
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if (Anim)
		{
			Anim->Montage_SetPosition(Montage, FMath::Clamp(GetPresentationServerTime() - ChargeState.StartServerTime,
				0.0f, FMath::Max(0.0f, Montage->GetPlayLength() - KINDA_SMALL_NUMBER)));
		}
	}
	if (ChargeState.bBow) { SetBowVisualAlpha(GetBowChargeAlpha()); }
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
	else
	{
		StopChargeProjectilePresentationLocal(); // Wait for the async equipment-ready notification.
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
