#pragma once

#include "CoreMinimal.h"
#include "Combat/Item/FEWeaponAmmoData.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Combat/Reaction/FECombatReactionTypes.h"
#include "Combat/Item/FEEquipmentInstanceData.h"
#include "FEWeaponItemData.generated.h"

class UAnimInstance;
class UAnimMontage;
class UGameplayAbility;
class UGameplayEffect;
class UStaticMesh;
class USkeletalMesh;
class UTexture2D;
class USoundBase;
class AFE_CombatProjectile;
class UFE_ImpactEffectData;
class UNiagaraSystem;
class UParticleSystem;
class UFE_CrossHairWidget;
class UFE_WeaponAttachmentData;
class UFE_WeaponSightData;

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

/** Accuracy penalty and lazy, shot-driven bloom. Intrinsic weapon spread remains separate. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AccuracySettings
{
	GENERATED_BODY()

	/** Extra cone half-angle at Accuracy 0. Accuracy 1 contributes zero penalty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Accuracy", meta=(ClampMin="0.0", Units="deg"))
	float MaxAccuracyPenaltyAngle = 0.0f;

	/** Cone angle added after every shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Accuracy", meta=(ClampMin="0.0", Units="deg"))
	float SpreadBloomPerShot = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Accuracy", meta=(ClampMin="0.0", Units="deg"))
	float MaxSpreadBloom = 0.0f;

	/** Bloom recovered per second. Recovery is evaluated only when another shot is fired. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Accuracy", meta=(ClampMin="0.0", Units="deg/s"))
	float SpreadRecoveryPerSecond = 0.0f;
};

/** Owning-client camera recoil. Actual hit/projectile direction stays server authoritative. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_RecoilSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0.0", Units="deg"))
	float MinPitch = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0.0", Units="deg"))
	float MaxPitch = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(Units="deg"))
	float MinYaw = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(Units="deg"))
	float MaxYaw = 0.0f;

	/** Smooths the upward kick, not a return to the original aim. Zero applies it instantly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0.0", Units="s"))
	float SmoothingTime = 0.04f;
};

/** ADS-only arms/weapon kick. Values describe one shot at RecoilControl=0, not camera recoil. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponVisualRecoilSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Recoil")
	bool bEnabled = true;

	/** Camera-local peak offset in cm: -X pulls toward the camera, +Z lifts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Recoil", meta=(Units="cm"))
	FVector KickTranslation = FVector(-1.5f, 0.0f, 0.1f);

	/** Peak pitch; yaw/roll are independently sampled within +/- their magnitudes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Recoil", meta=(Units="deg"))
	FRotator KickRotation = FRotator(0.8f, 0.1f, 0.1f);

	/** Rotation pivot relative to the camera, in cm. Does not move the camera itself. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Recoil", meta=(Units="cm"))
	FVector PivotOffset = FVector(-15.0f, 0.0f, -5.0f);

	/** Approximate recovery duration. The impulse peaks near RecoveryTime / 6. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Recoil", meta=(ClampMin="0.03", ClampMax="2.0", Units="s"))
	float RecoveryTime = 0.18f;

	/** Bounds accumulated displacement and impulse velocity during automatic fire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual Recoil", meta=(ClampMin="1.0", ClampMax="10.0"))
	float MaxStack = 3.0f;
};

/** Camera-local cosmetic motion kept separate from exact optic alignment and ballistic spread. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponAimSwaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway")
	bool bEnabled = true;

	/** Position lag per degree/second of camera rotation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway", meta=(ClampMin="0.0"))
	float LookLocationStrength = 0.002f;
	/** Rotation lag per degree/second of camera rotation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway", meta=(ClampMin="0.0"))
	float LookRotationStrength = 0.002f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway", meta=(ClampMin="0.0", Units="deg/s"))
	float MaxLookSpeed = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway", meta=(Units="cm"))
	FVector MovementLocationAmplitude = FVector(0.0f, 0.3f, 0.2f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway", meta=(Units="deg"))
	FRotator MovementRotationAmplitude = FRotator(0.15f, 0.1f, 0.2f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway", meta=(ClampMin="0.0", Units="Hz"))
	float MovementFrequency = 1.8f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway", meta=(ClampMin="0.0", Units="s"))
	float SmoothingTime = 0.08f;
	/** Applied to both positional and rotational sway while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Aim Sway", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AimMultiplier = 0.15f;
};

/** Reaction values applied after a damage execution succeeds. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AttackReactionData
{
	GENERATED_BODY()

	/** None never interrupts or stuns; knockback is evaluated independently. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack Reaction")
	EFE_StaggerStrength StaggerStrength = EFE_StaggerStrength::Light;

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

	/** Optional montage played by the equipped skeletal weapon mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Weapon Mesh")
	TObjectPtr<UAnimMontage> WeaponMeshAttackMontage;

	/** Hit-reaction values applied after DamageExecutionCalculation succeeds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Reaction")
	FFE_AttackReactionData AttackReactionData;

	/** Protection while this attack ability is active, including combo-step changes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Reaction")
	EFE_StaggerStrength IgnoreStaggerUpTo = EFE_StaggerStrength::None;

	/** If empty, the weapon's DefaultDamageEffect is used by the attack ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageEffectOverride;

	/** The ability granted while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayAbility"))
	TSoftClassPtr<UGameplayAbility> AbilityClass;
};

/** Camera-directed melee volume, independent of weapon animation and mesh sockets. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_MeleeAttackData : public UFE_WeaponAttackData
{
	GENERATED_BODY()

public:
	/** Forward reach measured from the camera to the front of the swept volume. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="0.0", Units="cm"))
	float TraceRange = 200.0f;

	/** Camera-local half sizes: X = depth, Y = horizontal width, Z = vertical height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="0.1", Units="cm"))
	FVector TraceHalfExtent = FVector(10.0f, 30.0f, 30.0f);

	/** Final damage scale for box-only enemies. Center-ray hits and world hits use x1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Damage", meta=(ClampMin="0.0", ClampMax="1.0"))
	float IndirectDamageMultiplier = 0.4f;

	/** Shows the configured melee reach and full swept box at the single hit notify. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace|Debug", meta=(DisplayName="Debug Trace"))
	bool bDrawDebugTrace = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace|Debug", meta=(ClampMin="0.0", Units="s", EditCondition="bDrawDebugTrace"))
	float DebugTraceDuration = 0.0f;

	/** Maximum unique enemy-area targets per swing; the center-only world target has a separate budget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Trace", meta=(ClampMin="1", DisplayName="Max Target Count"))
	int32 MaxHitCount = 1;

	/** Fallback interval when AttackResetEventTag is not configured. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Timing", meta=(ClampMin="0.01"))
	float AttackInterval = 0.4f;

	/** AnimNotify gameplay event that executes one camera trace. If unset, traces once at attack start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Melee|Timing", meta=(Categories="GameplayEvent"))
	FGameplayTag AttackStartEventTag;

	/** Optional legacy end event: suppresses a late hit notify, but does not reset the attack. */
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan|Accuracy")
	FFE_AccuracySettings AccuracySettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan|Recoil")
	FFE_RecoilSettings RecoilSettings;

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

	/** Draws the authoritative pellet trace. Red means blocking hit; green means no hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan|Debug")
	bool bDrawDebugTrace = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan|Debug",
		meta=(EditCondition="bDrawDebugTrace", ClampMin="0.0", Units="s"))
	float DebugTraceDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hitscan|Debug",
		meta=(EditCondition="bDrawDebugTrace", ClampMin="0.0"))
	float DebugTraceThickness = 1.5f;
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

	/** Maximum camera-center trace distance. A miss uses the trace end as the aim point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Aim", meta=(ClampMin="0.0", Units="cm"))
	float AimTraceRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Timing", meta=(ClampMin="0.01"))
	float FireInterval = 0.1f;

	/** Intrinsic projectile cone half-angle, independent of the Accuracy attribute. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Accuracy", meta=(ClampMin="0.0", Units="deg"))
	float SpreadAngle = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Accuracy")
	FFE_AccuracySettings AccuracySettings;

	/** Draws camera aim, socket convergence, and final spread direction on authority. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Debug")
	bool bDrawDebugAim = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Debug",
		meta=(EditCondition="bDrawDebugAim", ClampMin="0.0", Units="s"))
	float DebugAimDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Debug",
		meta=(EditCondition="bDrawDebugAim", ClampMin="0.0"))
	float DebugAimThickness = 1.5f;
};

/** Immediate projectile parameters. Launch speed is fixed per attack. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_ProjectileAttackData : public UFE_ProjectileAttackDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Launch", meta=(ClampMin="0.0"))
	float InitialSpeed = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Projectile|Recoil")
	FFE_RecoilSettings RecoilSettings;
};

/** Hold-to-charge projectile parameters shared by bows and throwable weapons. */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
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

	/**
	 * Gameplay launch origin for CharacterMesh throwables, relative to the character actor.
	 * This deliberately does not follow the animated hand socket. WeaponMesh attacks keep
	 * using ProjectileSpawnSocketName for both prediction and spawning.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Charged Projectile|Launch",
		meta=(EditCondition="AttachmentTarget == EFE_ChargedProjectileAttachmentTarget::CharacterMesh", MakeEditWidget))
	FVector FixedThrowStartOffset = FVector(50.0f, 25.0f, 65.0f);


};

/** Bow-only charged presentation. Camera and charge animation are not used by throwables. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_BowAttackData : public UFE_ChargedProjectileAttackData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bow|Camera", meta=(MakeEditWidget))
	FTransform ChargeCameraRelativeTransform = FTransform(
		FRotator(1.782045, 80.778150, -89.150872),
		FVector(12.977573, 71.894966, -38.414821),
		FVector::OneVector);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bow|Camera", meta=(ClampMin="0.0", Units="s"))
	float ChargeCameraBlendInTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bow|Camera", meta=(ClampMin="0.0", Units="s"))
	float ChargeCameraBlendOutTime = 0.2f;
};

/** Throwable-only charged presentation. The trajectory is local and never replicated. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class FALLENERA_API UFE_GrenadeAttackData : public UFE_ChargedProjectileAttackData
{
	GENERATED_BODY()

public:
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

/** Shared immutable weapon definition. AttackActions are asset-owned, not inventory instances. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponTemplateData
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Ammo")
	FFE_WeaponAmmoSettings AmmoSettings;
	/** Persistent attack-power contribution applied while this weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Stats")
	FFE_WeaponStat WeaponStat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Tags")
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Tags")
	FGameplayTagContainer WeaponTags;

	/** Granted to the owner's ASC only while equipped, e.g. Equipment.Weapon.Hammer. Classification tags are not automatically granted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Tags", meta=(Categories="Equipment"))
	FGameplayTagContainer EquippedTags;

	/** Static weapon mesh. Used only when SkeletalMesh is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation", meta=(DisplayName="Static Mesh"))
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Optional animated weapon mesh. When assigned, this takes priority over Mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftClassPtr<UAnimInstance> WeaponAnimLayerClass;

	/** Per-weapon crosshair widget. The HUD replaces its active child when equipment changes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftClassPtr<UFE_CrossHairWidget> CrosshairWidgetClass;

	/** AnimBP assigned directly to both equipped skeletal weapon meshes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	TSoftClassPtr<UAnimInstance> WeaponMeshAnimInstanceClass;

	/** Spatial sound played once whenever this weapon performs an attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Attack")
	TSoftObjectPtr<USoundBase> AttackSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Attack", meta=(ClampMin="0.0"))
	float AttackSoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Attack", meta=(ClampMin="0.0"))
	float AttackSoundPitch = 1.0f;

	/** AI hearing loudness emitted by an authoritative attack that has an AttackSound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Attack|AI", meta=(ClampMin="0.0"))
	float AttackNoiseLoudness = 1.0f;

	/** Optional hard cap for AI hearing. Zero uses the listener's HearingRange. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Attack|AI", meta=(ClampMin="0.0"))
	float AttackNoiseMaxRange = 0.0f;

	/** Surface-specific Niagara and sound presentation used by GameplayCue.Impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Impact")
	TObjectPtr<UFE_ImpactEffectData> ImpactEffectData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	FName AttachSocketName = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation")
	FTransform SocketOffset;

	/** Absolute first-person arms transform relative to the camera, applied on equip. Not a weapon socket offset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|FirstPerson")
	FTransform FirstPersonMeshTransform = FTransform(FRotator(0.0f, -90.0f, 0.0f), FVector(0.0f, 0.0f, -160.0f));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Damage", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DefaultDamageEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category="Weapon|Attacks")
	TArray<TObjectPtr<UFE_WeaponAttackData>> AttackActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Melee")
	EFE_MeleeAttackSequenceMode AttackSequenceMode = EFE_MeleeAttackSequenceMode::Combo;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Aim")
	FFE_WeaponAimSwaySettings AimSwaySettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Visual Recoil")
	FFE_WeaponVisualRecoilSettings VisualRecoilSettings;

	/** Opt in for firearms; bows/throwables keep their existing right-click behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attachments")
	bool bSupportsAiming = false;

	/** Default parts for the current DA-based test inventory. Runtime inventory uses SetWeaponAttachments. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attachments")
	TArray<TSoftObjectPtr<UFE_WeaponAttachmentData>> DefaultAttachments;

	/** Built-in sight when no sight part is equipped. Applies its accuracy bonus, but does not mount a mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Attachments")
	TSoftObjectPtr<UFE_WeaponSightData> DefaultSight;

	/** Optional muzzle flash spawned at the active ranged attack's socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Muzzle")
	TSoftObjectPtr<UNiagaraSystem> MuzzleNiagaraSystem;

	/** Legacy Cascade fallback used only when MuzzleNiagaraSystem is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Muzzle")
	TSoftObjectPtr<UParticleSystem> MuzzleParticleSystem;

	/** Shared scale for either muzzle Niagara or Particle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Presentation|Muzzle", meta=(DisplayName="Muzzle VFX Scale"))
	FVector MuzzleNiagaraScale = FVector::OneVector;
};

/** DA Instance is an editable test seed; equipment copies it instead of mutating the DA. */
UCLASS(Abstract, BlueprintType)
class FALLENERA_API UFE_WeaponItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Template")
	FFE_WeaponTemplateData Template;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Instance")
	FFE_WeaponInstanceData Instance;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	UFUNCTION(BlueprintPure, Category="Weapon")
	const UFE_WeaponAttackData* FindAttackForInputTag(FGameplayTag InInputTag) const;
	UFUNCTION(BlueprintPure, Category="Weapon")
	bool HasItemTag(FGameplayTag InItemTag) const;
protected:
	bool NeedsTemplateMigration() const { return ItemDataVersion == 0; }
private:
	UPROPERTY()
	int32 ItemDataVersion = 0;
	// Load-only legacy fields. Gameplay exclusively reads Template and Instance.
	/** Persistent attack-power contribution applied while this weapon is equipped. */
	UPROPERTY(meta=(DeprecatedProperty))
	FFE_WeaponStat WeaponStat;

	UPROPERTY(meta=(DeprecatedProperty))
	FGameplayTagContainer ItemTags;

	UPROPERTY(meta=(DeprecatedProperty))
	FGameplayTagContainer WeaponTags;

	/** Static weapon mesh. Used only when SkeletalMesh is empty. */
	UPROPERTY(meta=(DeprecatedProperty))
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Optional animated weapon mesh. When assigned, this takes priority over Mesh. */
	UPROPERTY(meta=(DeprecatedProperty))
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(meta=(DeprecatedProperty))
	TSoftClassPtr<UAnimInstance> WeaponAnimLayerClass;

	/** Per-weapon crosshair widget. The HUD replaces its active child when equipment changes. */
	UPROPERTY(meta=(DeprecatedProperty))
	TSoftClassPtr<UFE_CrossHairWidget> CrosshairWidgetClass;

	/** AnimBP assigned directly to both equipped skeletal weapon meshes. */
	UPROPERTY(meta=(DeprecatedProperty))
	TSoftClassPtr<UAnimInstance> WeaponMeshAnimInstanceClass;

	/** Spatial sound played once whenever this weapon performs an attack. */
	UPROPERTY(meta=(DeprecatedProperty))
	TSoftObjectPtr<USoundBase> AttackSound;

	UPROPERTY(meta=(DeprecatedProperty))
	float AttackSoundVolume = 1.0f;

	UPROPERTY(meta=(DeprecatedProperty))
	float AttackSoundPitch = 1.0f;

	/** AI hearing loudness emitted by an authoritative attack that has an AttackSound. */
	UPROPERTY(meta=(DeprecatedProperty))
	float AttackNoiseLoudness = 1.0f;

	/** Optional hard cap for AI hearing. Zero uses the listener's HearingRange. */
	UPROPERTY(meta=(DeprecatedProperty))
	float AttackNoiseMaxRange = 0.0f;

	/** Surface-specific Niagara and sound presentation used by GameplayCue.Impact. */
	UPROPERTY(meta=(DeprecatedProperty))
	TObjectPtr<UFE_ImpactEffectData> ImpactEffectData;

	UPROPERTY(meta=(DeprecatedProperty))
	FName AttachSocketName = TEXT("hand_r");

	UPROPERTY(meta=(DeprecatedProperty))
	FTransform SocketOffset;

	/** Absolute first-person arms transform relative to the camera, applied on equip. Not a weapon socket offset. */
	UPROPERTY(meta=(DeprecatedProperty))
	FTransform FirstPersonMeshTransform = FTransform(FRotator(0.0f, -90.0f, 0.0f), FVector(0.0f, 0.0f, -160.0f));

	UPROPERTY(meta=(DeprecatedProperty))
	TSoftClassPtr<UGameplayEffect> DefaultDamageEffect;

	UPROPERTY(meta=(DeprecatedProperty))
	TArray<TObjectPtr<UFE_WeaponAttackData>> AttackActions;

};

UCLASS(BlueprintType)
class FALLENERA_API UFE_MeleeWeaponItemData : public UFE_WeaponItemData
{
	GENERATED_BODY()
public:
	virtual void PostLoad() override;
private:
	UPROPERTY(meta=(DeprecatedProperty))
	EFE_MeleeAttackSequenceMode AttackSequenceMode = EFE_MeleeAttackSequenceMode::Combo;
};

UCLASS(BlueprintType)
class FALLENERA_API UFE_RangedWeaponItemData : public UFE_WeaponItemData
{
	GENERATED_BODY()
public:
	virtual void PostLoad() override;
private:
	UPROPERTY(meta=(DeprecatedProperty))
	FFE_WeaponAimSwaySettings AimSwaySettings;

	/** Opt in for firearms; bows/throwables keep their existing right-click behavior. */
	UPROPERTY(meta=(DeprecatedProperty))
	bool bSupportsAiming = false;

	/** Default parts for the current DA-based test inventory. Runtime inventory uses SetWeaponAttachments. */
	UPROPERTY(meta=(DeprecatedProperty))
	TArray<TSoftObjectPtr<UFE_WeaponAttachmentData>> DefaultAttachments;

	/** Built-in sight when no sight part is equipped. Applies its accuracy bonus, but does not mount a mesh. */
	UPROPERTY(meta=(DeprecatedProperty))
	TSoftObjectPtr<UFE_WeaponSightData> DefaultSight;

	/** Optional muzzle flash spawned at the active ranged attack's socket. */
	UPROPERTY(meta=(DeprecatedProperty))
	TSoftObjectPtr<UNiagaraSystem> MuzzleNiagaraSystem;

	/** Legacy Cascade fallback used only when MuzzleNiagaraSystem is empty. */
	UPROPERTY(meta=(DeprecatedProperty))
	TSoftObjectPtr<UParticleSystem> MuzzleParticleSystem;

	/** Shared scale for either muzzle Niagara or Particle. */
	UPROPERTY(meta=(DeprecatedProperty))
	FVector MuzzleNiagaraScale = FVector::OneVector;
};
