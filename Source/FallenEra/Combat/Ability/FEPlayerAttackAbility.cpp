#include "Combat/Ability/FEPlayerAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/FECombatGameplayTags.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

const UFE_WeaponItemData* UFE_PlayerAttackAbility::GetWeaponData(const AActor* Avatar)
{
	const UFE_EquipmentComponent* Equipment = Avatar ? Avatar->FindComponentByClass<UFE_EquipmentComponent>() : nullptr;
	return Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
}

UFE_PlayerAttackAbility::UFE_PlayerAttackAbility()
{
	ActivationPolicy = EFallenEraAbilityActivationPolicy::OnInputTriggered;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	FGameplayTagContainer AttackAbilityTags;
	AttackAbilityTags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Attack);
	SetAssetTags(AttackAbilityTags);
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
	CachedAttackData = nullptr;
	CachedWeaponData = nullptr;
	CachedDamageEffectClass = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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

	for (const UFE_WeaponAttackData* AttackData : WeaponData->AttackActions)
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
	CachedAttackData = const_cast<UFE_WeaponAttackData*>(AttackData);
	CachedDamageEffectClass = nullptr;
	if (!WeaponData || !AttackData)
	{
		return;
	}

	const TSoftClassPtr<UGameplayEffect>& EffectReference = AttackData->DamageEffectOverride.IsNull()
		? WeaponData->DefaultDamageEffect
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

	if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
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
		if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
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

	if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
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
	const FHitResult& HitResult) const
{
	if (!CombatCharacter || !WeaponData || !AttackData ||
		(!TargetActor && !HitResult.bBlockingHit))
	{
		return;
	}

	UFE_CombatComponent* CombatComponent = CombatCharacter->FindComponentByClass<UFE_CombatComponent>();
	if (!CombatComponent)
	{
		return;
	}

	const TSoftClassPtr<UGameplayEffect>& OverrideEffect = AttackData->DamageEffectOverride;
	const TSoftClassPtr<UGameplayEffect>& WeaponEffect = WeaponData->DefaultDamageEffect;
	if (UClass* DamageEffectClass = CachedDamageEffectClass
		? CachedDamageEffectClass.Get()
		: (OverrideEffect.IsNull() ? WeaponEffect.Get() : OverrideEffect.Get()))
	{
		CombatComponent->ApplyDamageWithEffectFromHit(
			TargetActor,
			DamageEffectClass,
			HitResult,
			AttackData);
	}
	else
	{
		CombatComponent->ApplyDamageFromHit(
			TargetActor,
			HitResult,
			AttackData);
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

	if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
	{
		Combat->ApplyLocalWeaponRecoil(RecoilSettings, RecoilControl);
	}
}
