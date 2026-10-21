#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/Component/FEProceduralHitReactionComponent.h"
#include "Combat/Component/FEKnockdownComponent.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Damage/FEHitZoneMappingData.h"
#include "Combat/Damage/FEHitZoneMultiplierData.h"
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
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarFEHitDamageDebug(
	TEXT("fe.Combat.DebugHitDamage"), 0,
	TEXT("Show server-confirmed bone, region multiplier and actual HP loss to the attacking player. 0=off, 1=on."),
	ECVF_Cheat);
#endif

UFE_CombatComponent::UFE_CombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
	DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
}

void UFE_CombatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bCombatConfigurationPending) { RefreshHitZoneData(); }
	RefreshAbilitySystem();
	if (UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>())
	{
		WeaponChangedHandle = Equipment->OnWeaponChanged().AddUObject(this, &ThisClass::HandleWeaponChanged);
	}
	if (bCombatConfigurationPending) { return; }
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
	if (bCombatConfigurationPending) { return; }
	CachedDamageEffectClass = DamageEffectClass.IsNull()
		? UFE_DamageGameplayEffect::StaticClass() : DamageEffectClass.Get();
	if (bDeathCollisionDisabled && !bDeathMontagePlayed && DeathMontage.IsValid())
	{
		PlayDeathMontage(DeathMontage.Get());
	}
}

void UFE_CombatComponent::SetPreloadedCombatConfiguration(
	TSoftClassPtr<UGameplayEffect> NewDamageEffect, TSoftObjectPtr<UAnimMontage> NewDeathMontage,
	TSoftObjectPtr<UFE_HitZoneMappingData> NewHitZoneMapping,
	TSoftObjectPtr<UFE_HitZoneMultiplierData> NewHitZoneMultipliers)
{
	if (CombatAssetLoadHandle) { CombatAssetLoadHandle->CancelHandle(); CombatAssetLoadHandle.Reset(); }
	DamageEffectClass = NewDamageEffect;
	DeathMontage = NewDeathMontage;
	HitZoneMappingData = NewHitZoneMapping;
	HitZoneMultiplierData = NewHitZoneMultipliers;
	bCombatConfigurationPending = false;
	RefreshHitZoneData();
	FinishLoadingCombatAssets();
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
	if (const UFE_KnockdownComponent* Knockdown = Actor->FindComponentByClass<UFE_KnockdownComponent>();
		Knockdown && Knockdown->IsKnockedDown()) { return false; }
	const UAbilitySystemComponent* ASC = FindAbilitySystemComponent(Actor);
	return !ASC || (!ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned) &&
		!ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown));
}

bool UFE_CombatComponent::IsActorAlive(AActor* Actor)
{
	if (!IsValid(Actor)) { return false; }
	const UAbilitySystemComponent* ASC = FindAbilitySystemComponent(Actor);
	return !ASC || !ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
}

void UFE_CombatComponent::CancelAttackActions()
{
	StopLocalWeaponRecoil();
	if (UFE_WeaponAimingComponent* Aiming = GetOwner()->FindComponentByClass<UFE_WeaponAimingComponent>())
	{
		Aiming->EndAim(true);
	}
	if (UAbilitySystemComponent* ASC = FindAbilitySystemComponent(GetOwner()))
	{
		FGameplayTagContainer AttackTags(FallenEraCombatGameplayTags::Ability_Combat_Attack);
		AttackTags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Aim);
		AttackTags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Reload);
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
	StopLocalWeaponRecoil();
	ClearStunState();
	AttackStaggerProtections.Reset();
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
	if (HitZoneLoadHandle) { HitZoneLoadHandle->CancelHandle(); HitZoneLoadHandle.Reset(); }
	CachedHitZoneMappingData = nullptr;
	CachedHitZoneMultiplierData = nullptr;
	HitZoneMeshComponent.Reset();
	StopChargeProjectilePresentationLocal();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StunTimerHandle);
		World->GetTimerManager().ClearTimer(CameraTransitionTimer);
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
	const UFE_WeaponAttackData* AttackData,
	EFE_DamageHitType HitType,
	float HitQualityMultiplier, float AmmoDamageMultiplier)
{
	return ApplyDamageInternal(TargetActor, CachedDamageEffectClass, &HitResult, AttackData, HitType, HitQualityMultiplier, AmmoDamageMultiplier);
}

bool UFE_CombatComponent::ApplyDamageWithEffect(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClassOverride)
{
	return ApplyDamageInternal(TargetActor, DamageEffectClassOverride);
}

bool UFE_CombatComponent::ApplyDamageWithEffectFromHit(
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> DamageEffectClassOverride,
	const FHitResult& HitResult,
	const UFE_WeaponAttackData* AttackData,
	EFE_DamageHitType HitType,
	float HitQualityMultiplier, float AmmoDamageMultiplier)
{
	return ApplyDamageInternal(
		TargetActor,
		DamageEffectClassOverride,
		&HitResult,
		AttackData,
		HitType,
		HitQualityMultiplier, AmmoDamageMultiplier);
}

void UFE_CombatComponent::RefreshHitZoneData()
{
	if (bCombatConfigurationPending) { return; }
	if (HitZoneLoadHandle) { HitZoneLoadHandle->CancelHandle(); HitZoneLoadHandle.Reset(); }
	CachedHitZoneMappingData = HitZoneMappingData.Get();
	CachedHitZoneMultiplierData = HitZoneMultiplierData.Get();
	PrepareHitZoneHierarchy();
	if (IsHitZoneDataReady()) { return; }
	TArray<FSoftObjectPath> Paths;
	GatherHitZoneAssetPaths(Paths);
	HitZoneLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingHitZoneData));
}

void UFE_CombatComponent::FinishLoadingHitZoneData()
{
	CachedHitZoneMappingData = HitZoneMappingData.Get();
	CachedHitZoneMultiplierData = HitZoneMultiplierData.Get();
	PrepareHitZoneHierarchy();
	if (!IsHitZoneDataReady())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not load its hit-zone assets. Direct hit damage is rejected until its rules are ready."),
			*GetNameSafe(GetOwner()));
	}
	HitZoneLoadHandle.Reset(); // Cached UPROPERTY references keep the shared assets alive.
}

void UFE_CombatComponent::PrepareHitZoneHierarchy()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	HitZoneMeshComponent = Character ? Character->GetMesh()
		: (GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr);
	if (CachedHitZoneMappingData && HitZoneMeshComponent.IsValid())
	{
		CachedHitZoneMappingData->PrepareForMesh(HitZoneMeshComponent->GetSkeletalMeshAsset());
	}
}

bool UFE_CombatComponent::IsHitZoneDataReady() const
{
	if (bCombatConfigurationPending) { return false; }
	USkeletalMesh* Mesh = HitZoneMeshComponent.IsValid() ? HitZoneMeshComponent->GetSkeletalMeshAsset() : nullptr;
	return (HitZoneMappingData.IsNull() || (CachedHitZoneMappingData && CachedHitZoneMappingData == HitZoneMappingData.Get() &&
		CachedHitZoneMappingData->IsPreparedForMesh(Mesh))) &&
		(HitZoneMultiplierData.IsNull() || (CachedHitZoneMultiplierData && CachedHitZoneMultiplierData == HitZoneMultiplierData.Get()));
}

void UFE_CombatComponent::GatherHitZoneAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	if (!HitZoneMappingData.IsNull()) { OutPaths.AddUnique(HitZoneMappingData.ToSoftObjectPath()); }
	if (!HitZoneMultiplierData.IsNull()) { OutPaths.AddUnique(HitZoneMultiplierData.ToSoftObjectPath()); }
}

float UFE_CombatComponent::ResolveHitRegionMultiplier(
	FName BoneName, EFE_DamageHitType HitType, FName& OutRegionName) const
{
	OutRegionName = HitType == EFE_DamageHitType::Area ? FName(TEXT("Area")) : FName(TEXT("Default"));
	if (HitType == EFE_DamageHitType::Area || BoneName.IsNone() || !IsHitZoneDataReady())
	{
		return 1.0f;
	}
	if (CachedHitZoneMappingData && !HitZoneMappingData.IsNull())
	{
		USkeletalMesh* Mesh = HitZoneMeshComponent.IsValid() ? HitZoneMeshComponent->GetSkeletalMeshAsset() : nullptr;
		if (const EFE_HitZone* Zone = CachedHitZoneMappingData->FindHitZone(BoneName, Mesh))
		{
			OutRegionName = UFE_HitZoneMappingData::GetZoneName(*Zone);
			return CachedHitZoneMultiplierData && !HitZoneMultiplierData.IsNull()
				? CachedHitZoneMultiplierData->GetDamageMultiplier(*Zone) : 1.0f;
		}
	}
	return 1.0f;
}

FHitResult UFE_CombatComponent::RefineDirectDamageHit(const FHitResult& Hit, float SweepRadius)
{
	AActor* Target = Hit.GetActor();
	if (!Target || !Hit.BoneName.IsNone()) { return Hit; }
	const ACharacter* Character = Cast<ACharacter>(Target);
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : Target->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh || !Mesh->GetPhysicsAsset()) { return Hit; }
	FHitResult BodyHit;
	bool bHitBody = false;
	if (SweepRadius > 0.0f)
	{
		bHitBody = Mesh->SweepComponent(BodyHit, Hit.TraceStart, Hit.TraceEnd, FQuat::Identity,
			FCollisionShape::MakeSphere(SweepRadius));
	}
	else if (!Hit.TraceStart.Equals(Hit.TraceEnd))
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FE_HitRegionRefinement), false);
		Params.bReturnPhysicalMaterial = true;
		bHitBody = Mesh->LineTraceComponent(BodyHit, Hit.TraceStart, Hit.TraceEnd, Params);
	}
	return bHitBody && !BodyHit.BoneName.IsNone() ? BodyHit : Hit;
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
	ApplyDamageReactionInternal(DamageSource, ReactionData, nullptr);
}

void UFE_CombatComponent::ApplyDamageReactionInternal(const AActor* DamageSource,
	const FFE_AttackReactionData& ReactionData, const FHitResult* HitResult)
{
	AActor* Victim = GetOwner();
	if (!Victim || !Victim->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* VictimAbilitySystem = FindAbilitySystemComponent(Victim);
	// A lethal hit already triggered the death montage from the AttributeSet.
	// Do not overwrite it with a knockback or hit-reaction montage afterwards.
	if (bDeathCollisionDisabled || !IsActorAlive(Victim)) { return; }
	if (const UFE_KnockdownComponent* Knockdown = Victim->FindComponentByClass<UFE_KnockdownComponent>();
		Knockdown && Knockdown->IsKnockedDown()) { return; }

	// Snapshot before EndAbility removes the cancelled attack's protection.
	const FFE_DamageReactionDecision Decision = ResolveDamageReaction(ReactionData);
	if (Decision.bInterruptAttack)
	{
		if (VictimAbilitySystem)
		{
			FGameplayTagContainer Tags(FallenEraCombatGameplayTags::Ability_Combat_Attack);
			if (Decision.bApplyStun) { Tags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Aim); }
			VictimAbilitySystem->CancelAbilities(&Tags);
		}
		if (Decision.bApplyStun)
		{
			if (UFE_WeaponAimingComponent* Aiming = Victim->FindComponentByClass<UFE_WeaponAimingComponent>())
			{
				Aiming->EndAim(true);
			}
		}
		StopChargeProjectilePresentation(false);
	}

	if (Decision.bApplyStun)
	{
		if (!bReactionStunActive)
		{
			if (ACharacter* VictimCharacter = Cast<ACharacter>(Victim))
			{
				if (UCharacterMovementComponent* MovementComponent = VictimCharacter->GetCharacterMovement())
				{
					MovementComponent->StopMovementImmediately();
				}
				if (AController* Controller = VictimCharacter->GetController()) { Controller->StopMovement(); }
			}
			VictimAbilitySystem->AddLooseGameplayTag(
				FallenEraGameplayTags::State_Stunned, 1, EGameplayTagReplicationState::TagAndCountToAll);
			bReactionStunActive = true;
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				StunTimerHandle, this, &UFE_CombatComponent::ClearStunState, ReactionData.StunDuration, false);
		}
	}

	// Knockback is independent of stagger protection and hit-montage immunity.
	UAnimMontage* HitMontage = nullptr;
	if (ACharacter* VictimCharacter = Cast<ACharacter>(Victim))
	{
		const float FinalKnockback = CalculateReceivedKnockback(ReactionData.KnockbackAmount);
		if (FinalKnockback > 0.0f && DamageSource && DamageSource != Victim)
		{
			FVector Direction = Victim->GetActorLocation() - DamageSource->GetActorLocation();
			Direction.Z = 0.0f;
			Direction = Direction.GetSafeNormal();
			if (Direction.IsNearlyZero()) { Direction = Victim->GetActorForwardVector(); }
			VictimCharacter->LaunchCharacter(Direction * FinalKnockback, true, true);
		}
		if (Decision.bPlayHitReaction && !HitReactionMontages.IsEmpty())
		{
			HitMontage = HitReactionMontages[FMath::RandHelper(HitReactionMontages.Num())];
		}
	}
	FFE_ProceduralHitReactionEvent Event;
	Event.bInterruptAttack = Decision.bInterruptAttack;
	const UFE_ProceduralHitReactionComponent* PhysicalReaction =
		Victim->FindComponentByClass<UFE_ProceduralHitReactionComponent>();
	// Cosmetic motion may accompany a resisted light hit without cancelling the attack.
	if (HitResult && !HitResult->BoneName.IsNone() && PhysicalReaction &&
		PhysicalReaction->IsConfiguredForHits() && CanActorPlayPhysicalHitReaction(Victim))
	{
		Event.HitBone = HitResult->BoneName;
		FVector Direction = (HitResult->TraceEnd - HitResult->TraceStart).GetSafeNormal();
		if (Direction.IsNearlyZero() && DamageSource)
		{
			Direction = (Victim->GetActorLocation() - DamageSource->GetActorLocation()).GetSafeNormal();
		}
		if (Direction.IsNearlyZero()) { Direction = -HitResult->ImpactNormal; }
		Event.Direction = Direction.GetSafeNormal();
	}
	if (Decision.bInterruptAttack || !Event.HitBone.IsNone()) { MulticastPlayDamageReaction(HitMontage, Event); }
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
		if (UFE_KnockdownComponent* Knockdown = Victim->FindComponentByClass<UFE_KnockdownComponent>())
		{
			Knockdown->StopForDeath();
		}
		// Stop first: ProcHitReact must not restore QueryOnly after death sets NoCollision.
		if (UFE_ProceduralHitReactionComponent* PhysicalReaction =
			Victim->FindComponentByClass<UFE_ProceduralHitReactionComponent>()) { PhysicalReaction->StopReaction(); }
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

void UFE_CombatComponent::MulticastPlayDamageReaction_Implementation(
	UAnimMontage* HitMontage, const FFE_ProceduralHitReactionEvent& Event)
{
	if (bDeathCollisionDisabled || !IsActorAlive(GetOwner())) { return; }
	if (const UFE_KnockdownComponent* Knockdown = GetOwner()->FindComponentByClass<UFE_KnockdownComponent>();
		Knockdown && Knockdown->IsKnockedDown()) { return; }
	// One event preserves stop-before-react ordering without stopping protected light-hit attacks.
	if (Event.bInterruptAttack) { StopWeaponActionPresentationLocal(); }
	if (!Event.HitBone.IsNone())
	{
		if (UFE_ProceduralHitReactionComponent* PhysicalReaction =
			GetOwner()->FindComponentByClass<UFE_ProceduralHitReactionComponent>())
		{
			if (PhysicalReaction->TryPlayReaction(Event)) { return; }
		}
	}
	PlayHitReactionMontage(HitMontage);
}

void UFE_CombatComponent::PlayHitReactionMontage(UAnimMontage* HitMontage)
{
	if (!HitMontage || bDeathCollisionDisabled)
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
	const UFE_WeaponAttackData* AttackData,
	EFE_DamageHitType HitType,
	float HitQualityMultiplier, float AmmoDamageMultiplier)
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
	DamageRequest.HitType = HitType;
	DamageRequest.AmmoDamageMultiplier = FMath::IsFinite(AmmoDamageMultiplier) ? FMath::Max(0.0f, AmmoDamageMultiplier) : 1.0f;
	DamageRequest.HitQualityMultiplier = FMath::IsFinite(HitQualityMultiplier)
		? FMath::Clamp(HitQualityMultiplier, 0.0f, 1.0f) : 0.0f;
	DamageRequest.bHasHitResult = HitResult != nullptr;
	if (HitResult)
	{
		DamageRequest.HitResult = HitType == EFE_DamageHitType::Direct
			? RefineDirectDamageHit(*HitResult) : *HitResult;
	}
	if (SourceAbilitySystem && SourceAbilitySystem->HasAttributeSetForAttribute(
		UFallenEraAttributeSet::GetAttackPowerAttribute()))
	{
		DamageRequest.SourceAttackPower = SourceAbilitySystem->GetNumericAttribute(
			UFallenEraAttributeSet::GetAttackPowerAttribute()) * DamageRequest.AmmoDamageMultiplier;
		if (!FMath::IsFinite(DamageRequest.SourceAttackPower)) { DamageRequest.SourceAttackPower = 0.0f; }
	}

	FFE_CombatDamageResult DamageResult;
	if (TargetActor && TargetActor->GetClass()->ImplementsInterface(UFE_Damageable::StaticClass()))
	{
		DamageResult = IFE_Damageable::Execute_ReceiveCombatDamage(TargetActor, DamageRequest);
	}
	if (DamageResult.bHandled)
	{
		ShowHitDamageDebug(DamageRequest, DamageResult);
	}

	if (HitResult && AttackData && !DamageResult.bImpactCueHandled && SourceAbilitySystem)
	{
		FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(const_cast<UFE_WeaponAttackData*>(AttackData));
		EffectContext.AddInstigator(SourceActor, SourceActor);
		EffectContext.AddHitResult(DamageRequest.HitResult, true);
		ExecuteImpactCue(SourceAbilitySystem, SourceActor, EffectContext, DamageRequest.HitResult, AttackData);
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
	if (DamageRequest.HitType == EFE_DamageHitType::Direct && DamageRequest.bHasHitResult && !IsHitZoneDataReady())
	{
		// Loading must not silently turn an intended headshot into x1. Area/non-positional damage is independent.
		Result.bHandled = true;
		Result.HitRegion = TEXT("NotReady");
		return Result;
	}
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
	Result.HitRegionMultiplier = ResolveHitRegionMultiplier(
		DamageRequest.bHasHitResult ? DamageRequest.HitResult.BoneName : NAME_None,
		DamageRequest.HitType, Result.HitRegion);
	EffectSpec.Data->SetSetByCallerMagnitude(
		FallenEraCombatGameplayTags::SetByCaller_Damage_HitRegionMultiplier, Result.HitRegionMultiplier);
	EffectSpec.Data->SetSetByCallerMagnitude(
		FallenEraCombatGameplayTags::SetByCaller_Damage_HitQualityMultiplier, DamageRequest.HitQualityMultiplier);
	EffectSpec.Data->SetSetByCallerMagnitude(
		FallenEraCombatGameplayTags::SetByCaller_Damage_AmmoMultiplier, DamageRequest.AmmoDamageMultiplier);

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
		Result.AppliedDamage = FMath::Max(0.0f, HealthBeforeDamage - HealthAfterDamage);
		UAISense_Damage::ReportDamageEvent(
			TargetActor,
			TargetActor,
			DamageRequest.SourceActor,
			Result.AppliedDamage,
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
	if (Result.AppliedDamage > 0.0f && DamageRequest.bHasHitResult && DamageRequest.HitType == EFE_DamageHitType::Direct)
	{
		if (UFE_KnockdownComponent* Knockdown = TargetActor->FindComponentByClass<UFE_KnockdownComponent>())
		{
			FVector Direction = (DamageRequest.HitResult.TraceEnd - DamageRequest.HitResult.TraceStart).GetSafeNormal();
			if (Direction.IsNearlyZero()) { Direction = -DamageRequest.HitResult.ImpactNormal; }
			if (Knockdown->TryStartFromHit(DamageRequest.HitResult.BoneName, Direction)) { return Result; }
		}
	}
	if (Result.bDamageApplied && DamageRequest.AttackData)
	{
		ApplyDamageReactionInternal(DamageRequest.SourceActor, DamageRequest.AttackData->AttackReactionData,
			DamageRequest.bHasHitResult && DamageRequest.HitType == EFE_DamageHitType::Direct
				? &DamageRequest.HitResult : nullptr);
	}
	return Result;
}

void UFE_CombatComponent::ShowHitDamageDebug(
	const FFE_CombatDamageRequest& Request, const FFE_CombatDamageResult& Result)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!CVarFEHitDamageDebug.GetValueOnGameThread()) { return; }
	const FString Bone = Request.HitType == EFE_DamageHitType::Area ? TEXT("N/A")
		: (Request.bHasHitResult ? Request.HitResult.BoneName.ToString() : TEXT("None"));
	const FString Message = FString::Printf(TEXT("Hit: %s | Bone: %s | Region: %s | x%.2f | Quality: x%.2f | Damage: %.2f%s"),
		*GetNameSafe(Request.TargetActor), *Bone, *Result.HitRegion.ToString(), Result.HitRegionMultiplier,
		Request.HitQualityMultiplier, Result.AppliedDamage, Result.bDamageApplied ? TEXT("") : TEXT(" (rejected)"));
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsPlayerControlled() && !Pawn->IsLocallyControlled())
	{
		ClientShowHitDamageDebug(Message);
	}
	else
	{
		ClientShowHitDamageDebug_Implementation(Message);
	}
#endif
}

void UFE_CombatComponent::ClientShowHitDamageDebug_Implementation(const FString& Message)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GEngine && GetNetMode() != NM_DedicatedServer)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 3.0f, FColor::Yellow, Message);
	}
#endif
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
