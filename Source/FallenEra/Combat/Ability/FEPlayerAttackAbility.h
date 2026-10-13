#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"
#include "FEPlayerAttackAbility.generated.h"

class ACharacter;
class UAnimMontage;
class UFE_WeaponAttackData;
class UFE_WeaponItemData;

/** Reusable attack ability that executes the attack data owned by the equipped weapon. */
UCLASS(Abstract, Blueprintable)
class FALLENERA_API UFE_PlayerAttackAbility : public UFallenEraGameplayAbility
{
	GENERATED_BODY()

public:
	UFE_PlayerAttackAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Animation")
	bool bPlayAttackMontage = true;

	/** Collision channel used by this attack type. Player attacks target the Enemy object channel by default. */
	virtual ECollisionChannel GetAttackTraceChannel() const;

	static const UFE_WeaponItemData* GetWeaponData(const AActor* Avatar);

	/** Resolves the AttackActions entry whose granted spec owns this activation. */
	const UFE_WeaponAttackData* ResolveAttackData(const ACharacter* CombatCharacter) const;
	const UFE_WeaponAttackData* ResolveAttackData(const FGameplayAbilitySpecHandle& Handle, const ACharacter* CombatCharacter) const;
	void CacheAttackContext(const UFE_WeaponItemData* WeaponData, const UFE_WeaponAttackData* AttackData);
	const UFE_WeaponAttackData* GetCachedAttackData() const { return CachedAttackData; }
	const UFE_WeaponItemData* GetCachedWeaponData() const { return CachedWeaponData; }
	TSubclassOf<UGameplayEffect> GetCachedDamageEffectClass() const { return CachedDamageEffectClass; }
	void PlayAttackMontage(const ACharacter* CombatCharacter, const UFE_WeaponAttackData* AttackData) const;
	void PlayAttackMontage(const ACharacter* CombatCharacter, UAnimMontage* Montage) const;
	void PlayAttackEffects(
		const ACharacter* CombatCharacter,
		const UFE_WeaponItemData* WeaponData,
		const UFE_WeaponAttackData* AttackData) const;
	void PlayAttackPresentation(
		const ACharacter* CombatCharacter,
		const UFE_WeaponItemData* WeaponData,
		const UFE_WeaponAttackData* AttackData) const;
	void ApplyDamage(
		const ACharacter* CombatCharacter,
		const UFE_WeaponItemData* WeaponData,
		const UFE_WeaponAttackData* AttackData,
		AActor* TargetActor,
		const FHitResult& HitResult) const;

	virtual void ExecuteAttack(const ACharacter* CombatCharacter, const UFE_WeaponItemData* WeaponData, const UFE_WeaponAttackData* AttackData) const;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponAttackData> CachedAttackData;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponItemData> CachedWeaponData;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> CachedDamageEffectClass;
};
