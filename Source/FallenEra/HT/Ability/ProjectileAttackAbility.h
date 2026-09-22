#pragma once

#include "CoreMinimal.h"
#include "HT/Ability/PlayerAttackAbility.h"
#include "ProjectileAttackAbility.generated.h"

class AFE_CombatProjectile;
class UFE_ProjectileAttackData;
class UFE_ProjectileAttackDataBase;

enum class EFE_ProjectileLaunchContext : uint8
{
	Authority,
	LocalPreview
};

/** Immediate single-shot projectile attack. Also exposes shared spawning to charged attacks. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_ProjectileAttackAbility : public UFE_PlayerAttackAbility
{
	GENERATED_BODY()

public:
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
	virtual void ExecuteAttack(
		const ACharacter* CombatCharacter,
		const UFE_WeaponItemData* WeaponData,
		const UFE_WeaponAttackData* AttackData) const override;

	bool CacheProjectileClass(const UFE_ProjectileAttackDataBase* AttackData);
	bool CanFireProjectile(const ACharacter* CombatCharacter, const UFE_ProjectileAttackDataBase* AttackData) const;
	void RecordProjectileFired(const ACharacter* CombatCharacter, const UFE_ProjectileAttackDataBase* AttackData);
	bool SpawnProjectile(
		const ACharacter* CombatCharacter,
		const UFE_WeaponItemData* WeaponData,
		const UFE_ProjectileAttackDataBase* AttackData,
		float LaunchSpeed) const;
	bool GetProjectileLaunchTransform(
		const ACharacter* CombatCharacter,
		const UFE_ProjectileAttackDataBase* AttackData,
		EFE_ProjectileLaunchContext LaunchContext,
		FVector& OutLocation,
		FVector& OutDirection) const;

	UPROPERTY(Transient)
	TSubclassOf<AFE_CombatProjectile> CachedProjectileClass;

	float NextFireTime = 0.0f;
};
