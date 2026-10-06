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

UFE_CombatComponent::UFE_CombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
}

void UFE_CombatComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshAbilitySystem();
	if (UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>())
	{
		WeaponChangedHandle = Equipment->OnWeaponChanged().AddUObject(this, &ThisClass::HandleWeaponChanged);
	}
	TArray<FSoftObjectPath> Paths;
	if (!DamageEffectClass.IsNull()) { Paths.Add(DamageEffectClass.ToSoftObjectPath()); }
	if (!DeathMontage.IsNull()) { Paths.Add(DeathMontage.ToSoftObjectPath()); }
	if (!Paths.IsEmpty())
	{
		CombatAssetLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingCombatAssets));
	}
	FinishLoadingCombatAssets();
}

void UFE_CombatComponent::FinishLoadingCombatAssets()
{
	CachedDamageEffectClass = DamageEffectClass.IsNull()
		? UFE_DamageGameplayEffect::StaticClass() : DamageEffectClass.Get();
	if (bDeathCollisionDisabled && !bDeathMontagePlayed && DeathMontage.IsValid())
	{
		PlayDeathMontage(DeathMontage.Get());
	}
}

void UFE_CombatComponent::RefreshAbilitySystem()
{
	UAbilitySystemComponent* ASC = FindAbilitySystemComponent(GetOwner());
	if (BoundAbilitySystem.Get() == ASC && DeathTagHandle.IsValid()) { return; }
	if (UAbilitySystemComponent* Previous = BoundAbilitySystem.Get())
	{
		Previous->RegisterGameplayTagEvent(FallenEraGameplayTags::State_Dead).Remove(DeathTagHandle);
	}
	BoundAbilitySystem = ASC;
	DeathTagHandle.Reset();
	if (ASC)
	{
		DeathTagHandle = ASC->RegisterGameplayTagEvent(FallenEraGameplayTags::State_Dead)
			.AddUObject(this, &ThisClass::HandleDeathTagChanged);
		HandleDeathTagChanged(FallenEraGameplayTags::State_Dead, ASC->GetTagCount(FallenEraGameplayTags::State_Dead));
	}
}

bool UFE_CombatComponent::CanActorAttack(AActor* Actor)
{
	if (!IsActorAlive(Actor)) { return false; }
	const UAbilitySystemComponent* ASC = FindAbilitySystemComponent(Actor);
	return !ASC || !ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned);
}

bool UFE_CombatComponent::IsActorAlive(AActor* Actor)
{
	if (!IsValid(Actor)) { return false; }
	const UAbilitySystemComponent* ASC = FindAbilitySystemComponent(Actor);
	return !ASC || !ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
}

void UFE_CombatComponent::CancelAttackActions()
{
	if (UAbilitySystemComponent* ASC = FindAbilitySystemComponent(GetOwner()))
	{
		FGameplayTagContainer AttackTags(FallenEraCombatGameplayTags::Ability_Combat_Attack);
		ASC->CancelAbilities(&AttackTags);
		if (UFallenEraAbilitySystemComponent* FEASC = Cast<UFallenEraAbilitySystemComponent>(ASC))
		{
			FEASC->ClearAbilityInput();
		}
	}
	StopChargeProjectilePresentation(false);
	StopWeaponActionPresentationLocal();
}

void UFE_CombatComponent::HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0) { return; }
	CancelAttackActions();
	if (GetOwner()->HasAuthority()) { HandleDeath(); }
}

float UFE_CombatComponent::GetPresentationServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
}

float UFE_CombatComponent::GetBowChargeAlpha() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const FFE_ChargePresentationState& State = Pawn && Pawn->IsLocallyControlled() ? LocalChargeState : ChargeState;
	return State.bBow && State.StartServerTime >= 0.0f
		? FMath::Clamp((GetPresentationServerTime() - State.StartServerTime) / FMath::Max(0.01f, State.FullChargeSeconds), 0.0f, 1.0f)
		: 0.0f;
}

bool UFE_CombatComponent::IsBowCharging() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const auto& State = Pawn && Pawn->IsLocallyControlled() ? LocalChargeState : ChargeState;
	return State.bBow && State.StartServerTime >= 0.0f && !bDeathCollisionDisabled;
}

void UFE_CombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearStunState();
	if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
	{
		ASC->RegisterGameplayTagEvent(FallenEraGameplayTags::State_Dead).Remove(DeathTagHandle);
	}
	if (UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>())
	{
		Equipment->OnWeaponChanged().Remove(WeaponChangedHandle);
	}
	if (CombatAssetLoadHandle) { CombatAssetLoadHandle->CancelHandle(); CombatAssetLoadHandle.Reset(); }
	if (ChargeAssetLoadHandle) { ChargeAssetLoadHandle->CancelHandle(); ChargeAssetLoadHandle.Reset(); }
	StopChargeProjectilePresentationLocal();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StunTimerHandle);
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
	DOREPLIFETIME(UFE_CombatComponent, DeathStartServerTime);
	DOREPLIFETIME(UFE_CombatComponent, ChargeState);
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

bool UFE_CombatComponent::ApplyDamage(AActor* TargetActor)
{
	return ApplyDamageInternal(TargetActor, CachedDamageEffectClass);
}

bool UFE_CombatComponent::ApplyDamageFromHit(
	AActor* TargetActor,
	const FHitResult& HitResult,
	const UFE_WeaponAttackData* AttackData)
{
	return ApplyDamageInternal(TargetActor, CachedDamageEffectClass, &HitResult, AttackData);
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
	if (!Victim || !Victim->HasAuthority() || bDeathCollisionDisabled)
	{
		return;
	}

	CancelAttackActions();
	DeathStartServerTime = GetPresentationServerTime();
	bDeathCollisionDisabled = true;
	OnRep_DeathCollisionDisabled();
	Victim->ForceNetUpdate();
}

void UFE_CombatComponent::OnRep_DeathCollisionDisabled()
{
	if (bDeathCollisionDisabled)
	{
		CancelAttackActions();
		ApplyDeathCollisionState();
		FinishLoadingCombatAssets();
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

void UFE_CombatComponent::PlayDeathMontage(UAnimMontage* Montage)
{
	if (!Montage || bDeathMontagePlayed || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	bDeathMontagePlayed = true;
	const float Position = FMath::Clamp(GetPresentationServerTime() - DeathStartServerTime,
		0.0f, FMath::Max(0.0f, Montage->GetPlayLength() - KINDA_SMALL_NUMBER));

	if (ACharacter* VictimCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = VictimCharacter->GetMesh())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(0.05f);
				AnimInstance->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, Position);
				if (FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage))
				{
					Instance->bEnableAutoBlendOut = false;
				}
			}
		}

		if (VictimCharacter->IsLocallyControlled())
		{
			if (USkeletalMeshComponent* FirstPersonMesh = IFE_CombatPresentation::FindFirstPersonMesh(VictimCharacter))
			{
				if (UAnimInstance* AnimInstance = FirstPersonMesh->GetAnimInstance())
				{
					AnimInstance->Montage_Stop(0.05f);
					AnimInstance->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, Position);
					if (FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage))
					{
						Instance->bEnableAutoBlendOut = false;
					}
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
	if (!IsActorAlive(SourceActor) || SourceActor == TargetActor ||
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
	if (!IsActorAlive(TargetActor) || !TargetActor->HasAuthority() ||
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
