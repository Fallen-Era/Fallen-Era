#include "Combat/Ability/FEPlayerAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

namespace
{
    template<typename T>
    T* ResolveAttackComponent(const AActor* Avatar, TWeakObjectPtr<T>& Cached)
    {
        if (!Avatar) { return nullptr; }
        if (!Cached.IsValid() || Cached->GetOwner() != Avatar) { Cached = Avatar->FindComponentByClass<T>(); }
        return Cached.Get();
    }
}

UFE_EquipmentComponent* UFE_PlayerAttackAbility::GetEquipmentComponent(const AActor* Avatar) const
{
    return ResolveAttackComponent(Avatar, CachedEquipment);
}
UFE_CombatComponent* UFE_PlayerAttackAbility::GetCombatComponent(const AActor* Avatar) const
{
    return ResolveAttackComponent(Avatar, CachedCombat);
}
UFE_WeaponAimingComponent* UFE_PlayerAttackAbility::GetAimingComponent(const AActor* Avatar) const
{
    return ResolveAttackComponent(Avatar, CachedAiming);
}

const UFE_WeaponItemData* UFE_PlayerAttackAbility::GetWeaponData(const AActor* Avatar) const
{
	const UFE_EquipmentComponent* Equipment = Avatar ? GetEquipmentComponent(Avatar) : nullptr;
	return Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
}

UFE_PlayerAttackAbility::UFE_PlayerAttackAbility()
{
	ActivationPolicy = EFallenEraAbilityActivationPolicy::OnInputTriggered;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	FGameplayTagContainer AttackAbilityTags;
	AttackAbilityTags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Attack);
	SetAssetTags(AttackAbilityTags);
	ActivationBlockedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_Reloading);
	InputRetryBlockedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_Reloading);
	ActivationBlockedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_SelectingAmmo);
}

bool UFE_PlayerAttackAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) { return false; }
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UFE_WeaponAttackData* Attack = ResolveAttackData(Handle, Character);
	if (!Attack || (!Attack->IsA<UFE_HitscanAttackData>() && !Attack->IsA<UFE_ProjectileAttackDataBase>())) { return true; }
	const UFE_WeaponItemData* Weapon = GetWeaponData(Character);
	const UFE_EquipmentComponent* Equipment = Character ? GetEquipmentComponent(Character) : nullptr;
	return Weapon && (!Weapon->Template.AmmoSettings.bUsesAmmo || (Equipment && Equipment->CanFireWeaponAmmo(Weapon)));
}

bool UFE_PlayerAttackAbility::HasLoadedWeaponAmmo() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UFE_EquipmentComponent* Equipment = Avatar ? GetEquipmentComponent(Avatar) : nullptr;
	return Equipment && Equipment->CanFireWeaponAmmo(GetCachedWeaponData());
}

bool UFE_PlayerAttackAbility::CommitWeaponShotCost()
{
	if (!HasLoadedWeaponAmmo() || !CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo)) { return false; }
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar->HasAuthority())
	{
		UFE_EquipmentComponent* Equipment = GetEquipmentComponent(Avatar);
		AmmoShotInstanceId = Equipment->GetWeaponInstanceId();
		if (!Equipment->ConsumeWeaponAmmo(GetCachedWeaponData(), ShotAmmo)) { return false; }
		bAmmoConsumedThisActivation = GetCachedWeaponData()->Template.AmmoSettings.bUsesAmmo;
	}
	return true;
}

void UFE_PlayerAttackAbility::RefundFailedProjectileAmmo()
{
	if (AActor* Avatar = GetAvatarActorFromActorInfo(); Avatar && Avatar->HasAuthority() && bAmmoConsumedThisActivation)
	{
		if (UFE_EquipmentComponent* Equipment = GetEquipmentComponent(Avatar))
		{
			Equipment->RefundWeaponAmmo(GetCachedWeaponData(), AmmoShotInstanceId, ShotAmmo);
		}
		bAmmoConsumedThisActivation = false;
	}
}

ECollisionChannel UFE_PlayerAttackAbility::GetAttackTraceChannel() const
{
	return FECollisionChannels::Enemy;
}

void UFE_PlayerAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? GetWeaponData(CombatCharacter) : nullptr;
	const UFE_WeaponAttackData* AttackData = ResolveAttackData(CombatCharacter);
	if (!CombatCharacter || !WeaponData || !AttackData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	PlayAttackPresentation(CombatCharacter, WeaponData, AttackData);
	CacheAttackContext(WeaponData, AttackData);
	if (CombatCharacter->HasAuthority())
	{
		ExecuteAttack(CombatCharacter, WeaponData, AttackData);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UFE_PlayerAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
	{
		if (UFE_CombatComponent* Combat = GetCombatComponent(Avatar))
		{
			Combat->ClearAttackStaggerProtection(this);
		}
	}
	const UFE_WeaponItemData* AmmoWeapon = CachedWeaponData;
	const bool bCharged = CachedAttackData && CachedAttackData->IsA<UFE_ChargedProjectileAttackData>();
	const bool bConsumed = bAmmoConsumedThisActivation;
	CachedAttackData = nullptr;
	CachedWeaponData = nullptr;
	CachedDamageEffectClass = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	if (AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr; !bWasCancelled && Avatar && Avatar->HasAuthority())
	{
		if (UFE_EquipmentComponent* Equipment = GetEquipmentComponent(Avatar))
		{
			Equipment->CompleteAmmoAttack(AmmoWeapon, bCharged, bConsumed);
		}
	}
	bAmmoConsumedThisActivation = false;
	ShotAmmo = FFE_AmmoRoundData();
}

const UFE_WeaponAttackData* UFE_PlayerAttackAbility::ResolveAttackData(const ACharacter* CombatCharacter) const
{
	return ResolveAttackData(CurrentSpecHandle, CombatCharacter);
}

const UFE_WeaponAttackData* UFE_PlayerAttackAbility::ResolveAttackData(
	const FGameplayAbilitySpecHandle& Handle,
	const ACharacter* CombatCharacter) const
{
	if (!CombatCharacter)
	{
		return nullptr;
	}

	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystem
		? AbilitySystem->FindAbilitySpecFromHandle(Handle)
		: nullptr;
	const UFE_WeaponItemData* WeaponData = AbilitySpec
		? Cast<UFE_WeaponItemData>(AbilitySpec->SourceObject.Get())
		: nullptr;
	if (!WeaponData)
	{
		WeaponData = GetWeaponData(CombatCharacter);
	}
	if (!WeaponData || !AbilitySpec)
	{
		return nullptr;
	}

	for (const UFE_WeaponAttackData* AttackData : WeaponData->Template.AttackActions)
	{
		if (!AttackData || AttackData->AbilityClass.Get() != GetClass())
		{
			continue;
		}

		if (!AttackData->InputTag.IsValid() || AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(AttackData->InputTag))
		{
			return AttackData;
		}
	}

	return nullptr;
}

void UFE_PlayerAttackAbility::CacheAttackContext(
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData)
{
	CachedWeaponData = const_cast<UFE_WeaponItemData*>(WeaponData);
	bAmmoConsumedThisActivation = false;
	CachedAttackData = const_cast<UFE_WeaponAttackData*>(AttackData);
	CachedDamageEffectClass = nullptr;
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UFE_CombatComponent* Combat = GetCombatComponent(Avatar))
		{
			Combat->SetAttackStaggerProtection(this, AttackData ? AttackData->IgnoreStaggerUpTo : EFE_StaggerStrength::None);
		}
	}
	if (!WeaponData || !AttackData)
	{
		return;
	}

	const TSoftClassPtr<UGameplayEffect>& EffectReference = AttackData->DamageEffectOverride.IsNull()
		? WeaponData->Template.DefaultDamageEffect
		: AttackData->DamageEffectOverride;
	CachedDamageEffectClass = EffectReference.Get();
}

void UFE_PlayerAttackAbility::PlayAttackMontage(
	const ACharacter* CombatCharacter,
	const UFE_WeaponAttackData* AttackData) const
{
	PlayAttackMontage(CombatCharacter, AttackData ? AttackData->AttackMontage.Get() : nullptr);
}

void UFE_PlayerAttackAbility::PlayAttackEffects(
	const ACharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData) const
{
	if (!CombatCharacter || !WeaponData || !AttackData)
	{
		return;
	}

	if (UFE_CombatComponent* Combat = GetCombatComponent(CombatCharacter))
	{
		Combat->PlayWeaponAttackEffects(WeaponData, AttackData,
			NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	}
}

void UFE_PlayerAttackAbility::PlayAttackPresentation(
	const ACharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData) const
{
	PlayAttackMontage(CombatCharacter, AttackData);
	if (CombatCharacter && AttackData && AttackData->WeaponMeshAttackMontage)
	{
		if (UFE_CombatComponent* Combat = GetCombatComponent(CombatCharacter))
		{
			Combat->PlayWeaponMeshMontage(
				AttackData->WeaponMeshAttackMontage,
				NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
		}
	}
	PlayAttackEffects(CombatCharacter, WeaponData, AttackData);
}

void UFE_PlayerAttackAbility::PlayAttackMontage(
	const ACharacter* CombatCharacter,
	UAnimMontage* Montage) const
{
	if (!bPlayAttackMontage || !CombatCharacter || !Montage)
	{
		return;
	}

	if (UFE_CombatComponent* Combat = GetCombatComponent(CombatCharacter))
	{
		Combat->PlayAttackMontage(Montage, NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	}
}

void UFE_PlayerAttackAbility::ExecuteAttack(
	const ACharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData) const
{
	(void)CombatCharacter;
	(void)WeaponData;
	(void)AttackData;
}

void UFE_PlayerAttackAbility::ApplyDamage(
	const ACharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData,
	AActor* TargetActor,
	const FHitResult& HitResult,
	float HitQualityMultiplier) const
{
	if (!CombatCharacter || !WeaponData || !AttackData ||
		(!TargetActor && !HitResult.bBlockingHit))
	{
		return;
	}

	UFE_CombatComponent* CombatComponent = GetCombatComponent(CombatCharacter);
	if (!CombatComponent)
	{
		return;
	}

	const TSoftClassPtr<UGameplayEffect>& OverrideEffect = AttackData->DamageEffectOverride;
	const TSoftClassPtr<UGameplayEffect>& WeaponEffect = WeaponData->Template.DefaultDamageEffect;
	if (UClass* DamageEffectClass = CachedDamageEffectClass
		? CachedDamageEffectClass.Get()
		: (OverrideEffect.IsNull() ? WeaponEffect.Get() : OverrideEffect.Get()))
	{
		CombatComponent->ApplyDamageWithEffectFromHit(
			TargetActor,
			DamageEffectClass,
			HitResult,
			AttackData,
			EFE_DamageHitType::Direct,
			HitQualityMultiplier, ShotAmmo.Instance.ResolveStats().DamageMultiplier);
	}
	else
	{
		CombatComponent->ApplyDamageFromHit(
			TargetActor,
			HitResult,
			AttackData,
			EFE_DamageHitType::Direct,
			HitQualityMultiplier, ShotAmmo.Instance.ResolveStats().DamageMultiplier);
	}
}

float UFE_PlayerAttackAbility::CalculateSpreadAngle(
	const ACharacter* CombatCharacter,
	const UFE_WeaponAttackData* AttackData,
	float IntrinsicSpreadAngle,
	const FFE_AccuracySettings& AccuracySettings) const
{
	const UWorld* World = CombatCharacter ? CombatCharacter->GetWorld() : nullptr;
	if (!World || !AttackData)
	{
		return FMath::Max(0.0f, IntrinsicSpreadAngle);
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (SpreadAttackData.Get() != AttackData)
	{
		SpreadAttackData = const_cast<UFE_WeaponAttackData*>(AttackData);
		CurrentSpreadBloom = 0.0f;
		LastSpreadUpdateTime = CurrentTime;
	}
	else
	{
		const float Elapsed = FMath::Max(0.0f, CurrentTime - LastSpreadUpdateTime);
		CurrentSpreadBloom = FMath::Max(
			0.0f,
			CurrentSpreadBloom - FMath::Max(0.0f, AccuracySettings.SpreadRecoveryPerSecond) * Elapsed);
		LastSpreadUpdateTime = CurrentTime;
	}

	float Accuracy = 1.0f;
	if (const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
		AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(UFallenEraAttributeSet::GetAccuracyAttribute()))
	{
		Accuracy = FMath::Clamp(
			AbilitySystem->GetNumericAttribute(UFallenEraAttributeSet::GetAccuracyAttribute()),
			0.0f,
			1.0f);
	}

	return FMath::Max(0.0f, IntrinsicSpreadAngle) +
		FMath::Max(0.0f, AccuracySettings.MaxAccuracyPenaltyAngle) * (1.0f - Accuracy) +
		CurrentSpreadBloom;
}

FVector UFE_PlayerAttackAbility::ApplySpreadToDirection(
	const FVector& Direction,
	float SpreadAngle) const
{
	const FVector NormalizedDirection = Direction.GetSafeNormal();
	return SpreadAngle > KINDA_SMALL_NUMBER
		? FMath::VRandCone(NormalizedDirection, FMath::DegreesToRadians(SpreadAngle))
		: NormalizedDirection;
}

void UFE_PlayerAttackAbility::CommitSpreadShot(
	const ACharacter* CombatCharacter,
	const UFE_WeaponAttackData* AttackData,
	const FFE_AccuracySettings& AccuracySettings) const
{
	// First update recovery at the exact shot time, then add one shot of bloom.
	CalculateSpreadAngle(CombatCharacter, AttackData, 0.0f, AccuracySettings);
	CurrentSpreadBloom = FMath::Clamp(
		CurrentSpreadBloom + FMath::Max(0.0f, AccuracySettings.SpreadBloomPerShot),
		0.0f,
		FMath::Max(0.0f, AccuracySettings.MaxSpreadBloom));
}

void UFE_PlayerAttackAbility::ApplyLocalRecoil(
	const ACharacter* CombatCharacter,
	const FFE_RecoilSettings& RecoilSettings) const
{
	if (!CombatCharacter || !CombatCharacter->IsLocallyControlled())
	{
		return;
	}

	float RecoilControl = 1.0f;
	if (const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
		AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(UFallenEraAttributeSet::GetRecoilControlAttribute()))
	{
		RecoilControl = AbilitySystem->GetNumericAttribute(
			UFallenEraAttributeSet::GetRecoilControlAttribute());
	}

	if (UFE_CombatComponent* Combat = GetCombatComponent(CombatCharacter))
	{
		Combat->ApplyLocalWeaponRecoil(RecoilSettings, RecoilControl);
	}
	if (const UFE_WeaponItemData* Weapon = GetCachedWeaponData())
	{
		if (UFE_WeaponAimingComponent* Aiming = GetAimingComponent(CombatCharacter))
		{
			Aiming->ApplyShotRecoil(Weapon->Template.VisualRecoilSettings, RecoilControl);
		}
	}
}
