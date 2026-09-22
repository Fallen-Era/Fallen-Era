#include "HT/Ability/PlayerAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Component/EquipmentComponent.h"
#include "HT/Weapon/WeaponItemData.h"

const UFE_WeaponItemData* UFE_PlayerAttackAbility::GetWeaponData(const AActor* Avatar)
{
	const UFE_EquipmentComponent* Equipment = Avatar ? Avatar->FindComponentByClass<UFE_EquipmentComponent>() : nullptr;
	return Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
}

UFE_PlayerAttackAbility::UFE_PlayerAttackAbility()
{
	ActivationPolicy = EFallenEraAbilityActivationPolicy::OnInputTriggered;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
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

	PlayAttackMontage(CombatCharacter, AttackData);
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
