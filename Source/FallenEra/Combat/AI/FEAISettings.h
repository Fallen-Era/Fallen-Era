#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/Condition/FECharacterConditionTypes.h"
#include "Combat/Reaction/FECombatReactionTypes.h"
#include "FEAISettings.generated.h"

class UAnimMontage;
class UBlendSpace;
class UGameplayEffect;
class UFE_EnemyMeleeAttackAbility;
class UFallenEraAbilitySet;
class UFE_ArmorItemData;
class UFE_HitZoneMappingData;
class UFE_HitZoneMultiplierData;

/** Startup GE: use Instant for initial attributes, Infinite/Duration for immunity and buffs. */
USTRUCT(BlueprintType)
struct FALLENERA_API FSAIStartupEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities")
	TSoftClassPtr<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities", meta=(ClampMin="0.0"))
	float EffectLevel = 1.0f;
};

/** Data-driven scoring used when several players are valid combat targets. */
USTRUCT(BlueprintType)
struct FALLENERA_API FSAITargetSelectionSettings
{
	GENERATED_BODY()

	/** Base score for a player that is currently visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float VisibleTargetScore = 100.0f;

	/** Maximum score added for proximity inside the sight radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float ProximityScore = 40.0f;

	/** Score added for each point of recent damage dealt to this enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float DamageScorePerPoint = 3.0f;

	/** Time constant used to decay damage threat. Zero disables retained damage threat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float DamageThreatDecaySeconds = 12.0f;

	/** Keeps an enemy on its current target unless another player is clearly more threatening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float CurrentTargetScore = 30.0f;

	/** Score removed per other enemy already assigned to this player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float TargetLoadPenalty = 20.0f;

	/** A challenger must exceed the current target by this amount before switching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float SwitchScoreAdvantage = 20.0f;

	/** Prevents rapid target changes immediately after acquiring a valid target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float MinimumTargetLockTime = 1.5f;

	/** How long a non-visible, stimulus-free player remains a target candidate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float TargetMemoryDuration = 8.0f;

	/** Other remembered players must be inside this radius before they can replace the current target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float TargetEvaluationRadius = 3000.0f;

	/** Small stable per-enemy preference that breaks equal-score ties without random retargeting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Target Selection", meta=(ClampMin="0.0"))
	float PreferenceVariance = 8.0f;
};

/** Montage-specific melee trace settings so each attack can use different weapon sockets. */
USTRUCT(BlueprintType)
struct FALLENERA_API FSAIAttackMontageSettings
{
	GENERATED_BODY()

	FSAIAttackMontageSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Default zombie attacks resist light hits, but heavy hits interrupt the montage and trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Reaction")
	EFE_StaggerStrength IgnoreStaggerUpTo = EFE_StaggerStrength::Light;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FName TraceStartSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FName TraceEndSocket = TEXT("hand_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(ClampMin="0.0"))
	float TraceRadius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(ClampMin="1"))
	int32 MaxHitTargets = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(Categories="GameplayEvent"))
	FGameplayTag TraceStartEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(Categories="GameplayEvent"))
	FGameplayTag TraceEndEventTag;

	/** Ends the attack lock. Montage length remains the bounded fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(Categories="GameplayEvent"))
	FGameplayTag AttackResetEventTag;
};

/** Complete reusable configuration cached by an enemy at BeginPlay. */
USTRUCT(BlueprintType)
struct FALLENERA_API FSAISettings
{
	GENERATED_BODY()

	FSAISettings();

	/** Automatically granted once. Do not also grant a different attack GA in an AbilitySet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Abilities")
	TSoftClassPtr<UFE_EnemyMeleeAttackAbility> AttackAbilityClass;

	/** Optional bundles for additional abilities/effects. Applied before StartupEffects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Abilities")
	TArray<TSoftObjectPtr<UFallenEraAbilitySet>> DefaultAbilitySets;

	/** Configure initialization/immunity GEs here without creating an AbilitySet. Never duplicate a GE in both lists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Abilities", meta=(TitleProperty="GameplayEffect"))
	TArray<FSAIStartupEffect> StartupEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equipment")
	TArray<TSoftObjectPtr<UFE_ArmorItemData>> DefaultArmorDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	TSoftObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage")
	TSoftObjectPtr<UFE_HitZoneMappingData> HitZoneMappingData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage")
	TSoftObjectPtr<UFE_HitZoneMultiplierData> HitZoneMultiplierData;

	/** Shared by runtime loading and spawn prewarming; server-only GAS assets are optional on clients. */
	void GatherAssetPaths(TArray<FSoftObjectPath>& OutPaths, bool bIncludeServerAssets) const;
	bool AreAssetsLoaded(bool bIncludeServerAssets) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(TitleProperty="AttackMontage"))
	TArray<FSAIAttackMontageSettings> AttackMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.01"))
	float AttackCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageEffect;

	/** Conditions independently rolled on the damaged target after this enemy deals health damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Conditions", meta=(TitleProperty="ConditionTag"))
	TArray<FFE_ConditionApplicationChance> ConditionApplicationChances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	TArray<TObjectPtr<UAnimMontage>> HitReactionMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float PatrolMovementSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float ChaseMovementSpeed = 450.0f;

	/** Multiplayer target selection can be tuned per enemy archetype. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Perception")
	FSAITargetSelectionSettings TargetSelection;
};
