#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "HT/Interface/Damageable.h"
#include "HT/Weapon/WeaponItemData.h"
#include "CombatComponent.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UAnimMontage;
class AFE_CombatProjectile;

/** Reusable, server-authoritative bridge for applying GameplayEffect damage to any GAS actor. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_CombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFE_CombatComponent();

	/** Authority multicasts; a predicting client only plays its local presentation. */
	void PlayAttackMontage(UAnimMontage* Montage, bool bPredictedByOwner);
	void PlayAttackMontageLocal(UAnimMontage* Montage);
	void StartChargeProjectilePresentation(
		TSubclassOf<AFE_CombatProjectile> ProjectileClass,
		EFE_ChargedProjectileAttachmentTarget AttachmentTarget,
		FName AttachSocketName,
		const FTransform& AttachOffset,
		bool bPredictedByOwner);
	void StopChargeProjectilePresentation(bool bPredictedByOwner);

	/** Applies the default damage GameplayEffect; execution captures AttackPower and DefensePower. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat")
	bool ApplyDamage(AActor* TargetActor);

	/** Applies damage and carries the trace hit plus weapon attack data into the effect context.
	 * If TargetActor has no ASC, only the replicated impact cue is emitted. */
	bool ApplyDamageFromHit(
		AActor* TargetActor,
		const FHitResult& HitResult,
		const UFE_WeaponAttackData* AttackData);

	/** Applies a custom damage GameplayEffect; damage is calculated from attributes by the effect. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat")
	bool ApplyDamageWithEffect(AActor* TargetActor, TSubclassOf<UGameplayEffect> DamageEffectClass);

	/** Applies a custom damage effect and carries the trace hit plus weapon attack data. */
	bool ApplyDamageWithEffectFromHit(
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		const FHitResult& HitResult,
		const UFE_WeaponAttackData* AttackData);

	/** GAS receiver implementation used by Damageable actors that own an ASC. */
	FFE_CombatDamageResult ApplyGameplayEffectDamage(const FFE_CombatDamageRequest& DamageRequest);

	/** Applies knockback, stun state, and a hit-reaction montage to this component's owner. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat|Reaction")
	void ApplyDamageReaction(const AActor* DamageSource, const FFE_AttackReactionData& ReactionData);

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Reaction")
	float CalculateReceivedKnockback(float IncomingKnockback) const;

	/** Plays the configured death montage once when this actor's Health reaches zero. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat|Death")
	void HandleDeath();

	/** Returns the ASC exposed by the actor, including PlayerState-owned ASCs. */
	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	static UAbilitySystemComponent* FindAbilitySystemComponent(AActor* Actor);

protected:
	/** Override this per character or weapon when a different damage GE is required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Reaction")
	TArray<TObjectPtr<UAnimMontage>> HitReactionMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

private:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttackMontage(UAnimMontage* Montage, bool bPredictedByOwner);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartChargeProjectilePresentation(
		TSubclassOf<AFE_CombatProjectile> ProjectileClass,
		EFE_ChargedProjectileAttachmentTarget AttachmentTarget,
		FName AttachSocketName,
		FTransform AttachOffset,
		bool bPredictedByOwner);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopChargeProjectilePresentation(bool bPredictedByOwner);

	void StartChargeProjectilePresentationLocal(
		TSubclassOf<AFE_CombatProjectile> ProjectileClass,
		EFE_ChargedProjectileAttachmentTarget AttachmentTarget,
		FName AttachSocketName,
		const FTransform& AttachOffset);
	void StopChargeProjectilePresentationLocal();

	bool ApplyDamageInternal(
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> EffectClass,
		const FHitResult* HitResult = nullptr,
		const UFE_WeaponAttackData* AttackData = nullptr);
	void ExecuteImpactCue(
		UAbilitySystemComponent* SourceAbilitySystem,
		AActor* SourceActor,
		const FGameplayEffectContextHandle& EffectContext,
		const FHitResult& HitResult,
		const UFE_WeaponAttackData* AttackData) const;
	void ClearStunState();
	void PlayHitReactionMontage(UAnimMontage* HitMontage);
	void PlayDeathMontage(UAnimMontage* Montage);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHitReaction(UAnimMontage* HitMontage);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDeathMontage(UAnimMontage* Montage);

	FTimerHandle StunTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<AFE_CombatProjectile> ChargeProjectilePreview;

	bool bReactionStunActive = false;
	bool bDeathMontagePlayed = false;
};
