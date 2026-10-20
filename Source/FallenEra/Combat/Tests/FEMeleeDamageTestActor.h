#pragma once

#include "Combat/Ability/FEMeleeAttackAbility.h"
#include "Combat/Ability/FEProjectileAttackAbility.h"
#include "Combat/Interface/FEDamageable.h"
#include "GameFramework/Actor.h"
#include "FEMeleeDamageTestActor.generated.h"

/** Non-ASC receiver verifies the same per-hit request used by inventory/world integrations. */
UCLASS(NotBlueprintable, Transient)
class AFE_MeleeDamageTestActor : public AActor, public IFE_Damageable
{
	GENERATED_BODY()
public:
	AFE_MeleeDamageTestActor();
	virtual FFE_CombatDamageResult ReceiveCombatDamage_Implementation(const FFE_CombatDamageRequest& Request) override;
	FFE_CombatDamageRequest LastRequest;
	int32 HitCount = 0;
	float Health = 1000.0f;
};

UCLASS(NotBlueprintable, Transient)
class UFE_MeleeDamageTestAbility : public UFE_MeleeAttackAbility
{
	GENERATED_BODY()
public:
	void RunTrace(const ACharacter* Character, const UFE_WeaponItemData* Weapon, const UFE_WeaponAttackData* Attack) const;
};

UCLASS(NotBlueprintable, Transient)
class UFE_ProjectileAimTestAbility : public UFE_ProjectileAttackAbility
{
	GENERATED_BODY()
public:
	bool GetPreviewLaunch(const ACharacter* Character, const UFE_ProjectileAttackDataBase* Attack,
		FVector& Location, FVector& Direction) const;
};
