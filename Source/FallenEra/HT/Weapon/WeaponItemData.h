#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WeaponItemData.generated.h"

class UAnimInstance;
class UAnimMontage;
class UGameplayAbility;
class UGameplayEffect;
class UStaticMesh;
class AActor;

UENUM(BlueprintType)
enum class EFE_MeleeAttackSequenceMode : uint8
{
	Combo,
	RandomSingle
};

/** One attack entry owned by a weapon data asset. */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_WeaponAttackData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGameplayTag AttackTag;

	/** This tag is added to the granted ability spec and is consumed by the tag-driven ASC. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(Categories="Ability.Input"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Damage magnitude supplied to the selected GameplayEffect for this attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Damage", meta=(ClampMin="0.0"))
	float DamageAmount = 10.0f;

	/** If empty, the weapon's DefaultDamageEffect is used by the attack ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageEffectOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(Categories="GameplayEvent"))
	FGameplayTag ExecutionEventTag;

	/** The ability granted while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayAbility"))
	TSoftClassPtr<UGameplayAbility> AbilityClass;
};

/** Extra trace parameters for a melee attack. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_MeleeAttackData : public UFE_WeaponAttackData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace")
	FName StartSocket;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace")
	FName EndSocket;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="0.0"))
	float TraceRadius = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace")
	bool bAllowMultipleHits = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="1"))
	int32 MaxHitCount = 1;

	/** Fallback interval when ExecutionEventTag is not configured. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Timing", meta=(ClampMin="0.01"))
	float AttackInterval = 0.4f;

	/** Repeats this attack sequence while the input is held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Timing")
	bool bAutomatic = true;
};

/** Extra line-trace parameters for a ranged hitscan attack. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_HitscanAttackData : public UFE_WeaponAttackData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan", meta=(ClampMin="0.0"))
	float TraceRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan", meta=(ClampMin="0.0"))
	float SpreadAngle = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan", meta=(ClampMin="1"))
	int32 PelletCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan", meta=(ClampMin="0"))
	int32 PenetrationCount = 0;

	/** Time between shots when this attack is automatic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan|Weapon", meta=(ClampMin="0.01"))
	float FireInterval = 0.1f;

	/** When true, the Ability remains active and fires while the input is held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan|Weapon")
	bool bAutomatic = true;
};

/** Extra projectile parameters for a ranged projectile attack. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_ProjectileAttackData : public UFE_WeaponAttackData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(AllowedClasses="/Script/Engine.Actor"))
	TSoftClassPtr<AActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	FName ProjectileSpawnSocketName = TEXT("ArrowSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.0"))
	float InitialSpeed = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	float GravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	bool bUseAimDirection = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Timing", meta=(ClampMin="0.01"))
	float FireInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Timing")
	bool bAutomatic = true;
};

/** Data-driven weapon definition. The equipped character only caches this object. */
UCLASS(Abstract, BlueprintType)
class FALLENERA_API UFE_WeaponItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Tags")
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Tags")
	FGameplayTagContainer WeaponTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftClassPtr<UAnimInstance> WeaponAnimLayerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	FName AttachSocketName = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	FTransform SocketOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Damage", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DefaultDamageEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category="Weapon|Attacks")
	TArray<TObjectPtr<UFE_WeaponAttackData>> AttackActions;

	UFUNCTION(BlueprintPure, Category="Weapon")
	const UFE_WeaponAttackData* FindAttackForInputTag(FGameplayTag InInputTag) const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool HasItemTag(FGameplayTag InItemTag) const;
};

/** Optional concrete asset type for melee weapons. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_MeleeWeaponItemData : public UFE_WeaponItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Melee")
	EFE_MeleeAttackSequenceMode AttackSequenceMode = EFE_MeleeAttackSequenceMode::Combo;
};

/** Optional concrete asset type for ranged weapons. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_RangedWeaponItemData : public UFE_WeaponItemData
{
	GENERATED_BODY()
};
