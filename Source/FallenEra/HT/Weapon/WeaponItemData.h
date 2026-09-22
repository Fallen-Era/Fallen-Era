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
class USkeletalMesh;
class UTexture2D;
class AFE_CombatProjectile;
class UFE_ImpactEffectData;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EFE_MeleeAttackSequenceMode : uint8
{
	Combo,
	RandomSingle
};

UENUM(BlueprintType)
enum class EFE_ChargedProjectileAttachmentTarget : uint8
{
	CharacterMesh,
	WeaponMesh
};

/** Persistent stats granted while a weapon is equipped. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponStat
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Stat", meta=(ClampMin="0.0"))
	float Offense = 0.0f;
};

/** Reaction values applied after a damage execution succeeds. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AttackReactionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack Reaction", meta=(ClampMin="0.0"))
	float KnockbackAmount = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack Reaction", meta=(ClampMin="0.0"))
	float StunDuration = 0.0f;
};

/** One attack entry owned by a weapon data asset. */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_WeaponAttackData : public UObject
{
	GENERATED_BODY()

public:
	/** Attack entries are package-owned instanced objects referenced by replicated effect contexts. */
	virtual bool IsSupportedForNetworking() const override { return true; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	FGameplayTag AttackTag;

	/** This tag is added to the granted ability spec and is consumed by the tag-driven ASC. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(Categories="Ability.Input"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Hit-reaction values applied after DamageExecutionCalculation succeeds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Reaction")
	FFE_AttackReactionData AttackReactionData;

	/** If empty, the weapon's DefaultDamageEffect is used by the attack ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageEffectOverride;

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

	/** Maximum unique targets affected by one attack window. Kept as MaxHitCount for existing assets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="1", DisplayName="Max Target Count"))
	int32 MaxHitCount = 1;

	/** Minimum seconds before the same target can be damaged again when multiple hits are enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="0.01", EditCondition="bAllowMultipleHits"))
	float RepeatedHitInterval = 0.1f;

	/** Fallback interval when AttackResetEventTag is not configured. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Timing", meta=(ClampMin="0.01"))
	float AttackInterval = 0.4f;

	/** AnimNotify gameplay event that starts the active socket trace window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Timing", meta=(Categories="GameplayEvent"))
	FGameplayTag AttackStartEventTag;

	/** AnimNotify gameplay event that ends the active socket trace window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Timing", meta=(Categories="GameplayEvent"))
	FGameplayTag AttackEndEventTag;

	/**
	 * AnimNotify gameplay event that marks the attack reset/next-attack window.
	 * If unset, the montage length/AttackInterval fallback is used.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Timing", meta=(Categories="GameplayEvent"))
	FGameplayTag AttackResetEventTag;

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

/** Parameters shared by every projectile attack, regardless of how launch speed is resolved. */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_ProjectileAttackDataBase : public UFE_WeaponAttackData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	TSoftClassPtr<AFE_CombatProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	FName ProjectileSpawnSocketName = TEXT("ArrowSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	float GravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile")
	bool bUseAimDirection = true;

	/** Camera trace distance used to resolve a world-space aim point before firing from the muzzle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile", meta=(ClampMin="0.0"))
	float AimTraceRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Timing", meta=(ClampMin="0.01"))
	float FireInterval = 0.1f;
};

/** Immediate projectile parameters. Launch speed is fixed per attack. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_ProjectileAttackData : public UFE_ProjectileAttackDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Launch", meta=(ClampMin="0.0"))
	float InitialSpeed = 3000.0f;
};

/** Hold-to-charge projectile parameters shared by bows and throwable weapons. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_ChargedProjectileAttackData : public UFE_ProjectileAttackDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Timing", meta=(ClampMin="0.01"))
	float MaxChargeTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Launch", meta=(ClampMin="0.0"))
	float MinLaunchSpeed = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Launch", meta=(ClampMin="0.0"))
	float MaxLaunchSpeed = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Animation")
	TObjectPtr<UAnimMontage> ChargeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Animation")
	TObjectPtr<UAnimMontage> ReleaseMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Presentation")
	EFE_ChargedProjectileAttachmentTarget AttachmentTarget = EFE_ChargedProjectileAttachmentTarget::WeaponMesh;

	/** Bow: a socket on the equipped bow. Throwable: a socket on the character hand. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Presentation")
	FName ChargeAttachSocketName = TEXT("ProjectilePreview");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Presentation")
	FTransform ChargeAttachOffset;

	/** Optional local-only trajectory Niagara. Leave empty for bows without a trajectory preview. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Trajectory")
	TSoftObjectPtr<UNiagaraSystem> TrajectoryNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Trajectory", meta=(ClampMin="0.016"))
	float TrajectoryUpdateInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Trajectory", meta=(ClampMin="0.1"))
	float TrajectorySimulationTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Trajectory", meta=(ClampMin="5.0", ClampMax="60.0"))
	float TrajectorySimulationFrequency = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Trajectory", meta=(ClampMin="0.0"))
	float TrajectoryCollisionRadius = 5.0f;
};

/** Data-driven weapon definition cached by EquipmentComponent while equipped. */
UCLASS(Abstract, BlueprintType)
class FALLENERA_API UFE_WeaponItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Persistent attack-power contribution applied while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Stats")
	FFE_WeaponStat WeaponStat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Tags")
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Tags")
	FGameplayTagContainer WeaponTags;

	/** Static weapon mesh. Used only when SkeletalMesh is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation", meta=(DisplayName="Static Mesh"))
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Optional animated weapon mesh. When assigned, this takes priority over Mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftClassPtr<UAnimInstance> WeaponAnimLayerClass;

	/** Crosshair texture shown while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftObjectPtr<UTexture2D> CrosshairTexture;

	/** Surface-specific Niagara and sound presentation used by GameplayCue.Impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Impact")
	TObjectPtr<UFE_ImpactEffectData> ImpactEffectData;

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
