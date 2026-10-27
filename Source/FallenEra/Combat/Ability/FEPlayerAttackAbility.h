#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"
#include "Combat/Item/FEWeaponAmmoData.h"
#include "FEPlayerAttackAbility.generated.h"

class ACharacter;
class UAnimMontage;
class UFE_WeaponAttackData;
class UFE_WeaponItemData;
class UFE_EquipmentComponent;
class UFE_CombatComponent;
class UFE_WeaponAimingComponent;
struct FFE_AccuracySettings;
struct FFE_RecoilSettings;

/** Reusable attack ability that executes the attack data owned by the equipped weapon. */
UCLASS(Abstract, Blueprintable)
class FALLENERA_API UFE_PlayerAttackAbility : public UFallenEraGameplayAbility
{
	GENERATED_BODY()

public:
	UFE_PlayerAttackAbility();
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

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
	/** GAS cost plus one round, once per shot (never per pellet or charge start). */
	bool CommitWeaponShotCost();
	void RefundFailedProjectileAmmo();
	bool HasLoadedWeaponAmmo() const;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Animation")
	bool bPlayAttackMontage = true;

	/** Collision channel used by this attack type. Player attacks target the Enemy object channel by default. */
	virtual ECollisionChannel GetAttackTraceChannel() const;

	const UFE_WeaponItemData* GetWeaponData(const AActor* Avatar) const;
	UFE_EquipmentComponent* GetEquipmentComponent(const AActor* Avatar) const;
	UFE_CombatComponent* GetCombatComponent(const AActor* Avatar) const;
	UFE_WeaponAimingComponent* GetAimingComponent(const AActor* Avatar) const;
	mutable TWeakObjectPtr<UFE_EquipmentComponent> CachedEquipment;
	mutable TWeakObjectPtr<UFE_CombatComponent> CachedCombat;
	mutable TWeakObjectPtr<UFE_WeaponAimingComponent> CachedAiming;

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
		const FHitResult& HitResult,
		float HitQualityMultiplier = 1.0f) const;

	/** Resolves intrinsic + attribute penalty + recovered bloom without advancing bloom. */
	float CalculateSpreadAngle(
		const ACharacter* CombatCharacter,
		const UFE_WeaponAttackData* AttackData,
		float IntrinsicSpreadAngle,
		const FFE_AccuracySettings& AccuracySettings) const;
	FVector ApplySpreadToDirection(const FVector& Direction, float SpreadAngle) const;
	void CommitSpreadShot(
		const ACharacter* CombatCharacter,
		const UFE_WeaponAttackData* AttackData,
		const FFE_AccuracySettings& AccuracySettings) const;
	void ApplyLocalRecoil(
		const ACharacter* CombatCharacter,
		const FFE_RecoilSettings& RecoilSettings) const;

	virtual void ExecuteAttack(const ACharacter* CombatCharacter, const UFE_WeaponItemData* WeaponData, const UFE_WeaponAttackData* AttackData) const;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponAttackData> CachedAttackData;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponItemData> CachedWeaponData;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> CachedDamageEffectClass;

	mutable TWeakObjectPtr<UFE_WeaponAttackData> SpreadAttackData;
	mutable float CurrentSpreadBloom = 0.0f;
	mutable float LastSpreadUpdateTime = 0.0f;
	bool bAmmoConsumedThisActivation = false;
	FGuid AmmoShotInstanceId;
	UPROPERTY(Transient) FFE_AmmoRoundData ShotAmmo;
};
