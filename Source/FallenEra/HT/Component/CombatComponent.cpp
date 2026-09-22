#include "HT/Component/CombatComponent.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "HT/Armor/ArmorGameplayTags.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HT/Interface/CombatPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HT/Component/EquipmentComponent.h"
#include "HT/Effect/DamageGameplayEffect.h"
#include "HT/Projectile/CombatProjectile.h"
#include "HT/Weapon/WeaponItemData.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UFE_CombatComponent::UFE_CombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
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
	ChargeProjectilePreview = Character->GetWorld()->SpawnActorDeferred<AFE_CombatProjectile>(
		ProjectileClass,
		PreviewTransform,
		Character,
		Character,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!ChargeProjectilePreview)
	{
		return;
	}

	ChargeProjectilePreview->ConfigureAsLocalPreview();
	UGameplayStatics::FinishSpawningActor(ChargeProjectilePreview, PreviewTransform);
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
		ChargeProjectilePreview->Destroy();
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
	if (ASC && ASC->HasMatchingGameplayTag(FE_ArmorGameplayTags::State_Immune_Knockback))
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
				if (ACharacter* VictimCharacter = Cast<ACharacter>(Victim))
				{
					if (UCharacterMovementComponent* MovementComponent = VictimCharacter->GetCharacterMovement())
					{
						MovementComponent->StopMovementImmediately();
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
	if (DeathMontage)
	{
		MulticastPlayDeathMontage(DeathMontage);
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
		!DamageRequest.DamageEffectClass)
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

	Result.bHandled = true;
	const FActiveGameplayEffectHandle AppliedHandle =
		TargetAbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	Result.bDamageApplied = AppliedHandle.WasSuccessfullyApplied();
	Result.bImpactCueHandled = Result.bDamageApplied && DamageRequest.bHasHitResult;
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
	SourceAbilitySystem->ExecuteGameplayCue(FallenEraGameplayTags::GameplayCue_Impact, CueParameters);
}
