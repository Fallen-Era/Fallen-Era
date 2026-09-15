#include "HT/Ability/PlayerAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "HT/Character/CombatCharacter.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Weapon/WeaponItemData.h"

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

	const AFE_CombatCharacter* CombatCharacter = Cast<AFE_CombatCharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? CombatCharacter->GetCurrentWeaponData() : nullptr;
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

const UFE_WeaponAttackData* UFE_PlayerAttackAbility::ResolveAttackData(const AFE_CombatCharacter* CombatCharacter) const
{
	return ResolveAttackData(CurrentSpecHandle, CombatCharacter);
}

const UFE_WeaponAttackData* UFE_PlayerAttackAbility::ResolveAttackData(
	const FGameplayAbilitySpecHandle& Handle,
	const AFE_CombatCharacter* CombatCharacter) const
{
	if (!CombatCharacter)
	{
		return nullptr;
	}

	const UFE_WeaponItemData* WeaponData = CombatCharacter->GetCurrentWeaponData();
	const UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystem
		? AbilitySystem->FindAbilitySpecFromHandle(Handle)
		: nullptr;
	if (!WeaponData || !AbilitySpec)
	{
		return nullptr;
	}

	for (const UFE_WeaponAttackData* AttackData : WeaponData->AttackActions)
	{
		if (!AttackData || AttackData->AbilityClass.LoadSynchronous() != GetClass())
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
	CachedDamageEffectClass = EffectReference.LoadSynchronous();
}

void UFE_PlayerAttackAbility::PlayAttackMontage(
	const AFE_CombatCharacter* CombatCharacter,
	const UFE_WeaponAttackData* AttackData) const
{
	if (!bPlayAttackMontage || !CombatCharacter || !AttackData || !AttackData->AttackMontage)
	{
		return;
	}

	if (USkeletalMeshComponent* WorldMesh = CombatCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = WorldMesh->GetAnimInstance())
		{
			AnimInstance->Montage_Play(AttackData->AttackMontage);
		}
	}

	if (CombatCharacter->IsLocallyControlled())
	{
		if (USkeletalMeshComponent* FirstPersonMesh = CombatCharacter->GetFirstPersonMesh())
		{
			if (UAnimInstance* AnimInstance = FirstPersonMesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(AttackData->AttackMontage);
			}
		}
	}
}

void UFE_PlayerAttackAbility::ExecuteAttack(
	const AFE_CombatCharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData) const
{
	(void)CombatCharacter;
	(void)WeaponData;
	(void)AttackData;
}

void UFE_PlayerAttackAbility::ApplyDamage(
	const AFE_CombatCharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData,
	AActor* TargetActor) const
{
	if (!CombatCharacter || !WeaponData || !AttackData || !TargetActor || AttackData->DamageAmount <= 0.0f)
	{
		return;
	}

	UFE_CombatComponent* CombatComponent = CombatCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	const TSoftClassPtr<UGameplayEffect>& OverrideEffect = AttackData->DamageEffectOverride;
	const TSoftClassPtr<UGameplayEffect>& WeaponEffect = WeaponData->DefaultDamageEffect;
	if (UClass* DamageEffectClass = CachedDamageEffectClass
		? CachedDamageEffectClass.Get()
		: (OverrideEffect.IsNull() ? WeaponEffect.LoadSynchronous() : OverrideEffect.LoadSynchronous()))
	{
		CombatComponent->ApplyDamageWithEffect(TargetActor, AttackData->DamageAmount, DamageEffectClass);
	}
	else
	{
		CombatComponent->ApplyDamage(TargetActor, AttackData->DamageAmount);
	}
}
