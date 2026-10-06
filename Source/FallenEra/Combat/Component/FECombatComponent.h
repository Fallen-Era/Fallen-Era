#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Combat/Interface/FEDamageable.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "FECombatComponent.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UAnimMontage;
class UNiagaraSystem;
class UParticleSystem;
class USoundBase;
class AFE_CombatProjectile;
class UCameraComponent;
struct FStreamableHandle;

/** Persistent presentation state: no per-frame alpha replication or multicast history. */
USTRUCT()
struct FFE_ChargePresentationState
{
	GENERATED_BODY()
	UPROPERTY() float StartServerTime = -1.0f;
	UPROPERTY() float FullChargeSeconds = 1.0f;
	UPROPERTY() bool bBow = false;
	UPROPERTY() TSoftClassPtr<AFE_CombatProjectile> ProjectileClass;
	UPROPERTY() TSoftObjectPtr<UAnimMontage> Montage;
	UPROPERTY() EFE_ChargedProjectileAttachmentTarget AttachmentTarget = EFE_ChargedProjectileAttachmentTarget::WeaponMesh;
	UPROPERTY() FName SocketName;
	UPROPERTY() FTransform Offset;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFEBowChargeChanged, float);

/** Reusable, server-authoritative bridge for applying GameplayEffect damage to any GAS actor. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_CombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFE_CombatComponent();
	virtual void BeginPlay() override;
	/** Rebind after PlayerState ASC actor info becomes available. */
	void RefreshAbilitySystem();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority multicasts; a predicting client only plays its local presentation. */
	void PlayAttackMontage(UAnimMontage* Montage, bool bPredictedByOwner);
	void PlayAttackMontageLocal(UAnimMontage* Montage);
	void PlayWeaponMeshMontage(UAnimMontage* Montage, bool bPredictedByOwner);
	void PlayWeaponAttackEffects(
		const UFE_WeaponItemData* WeaponData, const UFE_WeaponAttackData* AttackData, bool bPredictedByOwner);

	/** Reports a server-authoritative hearing stimulus. Use for footsteps and non-weapon sounds. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Combat|AI")
	void ReportCombatNoise(float Loudness = 1.0f, float MaxRange = 0.0f, FName NoiseTag = NAME_None);
	void StartChargeProjectilePresentation(
		TSubclassOf<AFE_CombatProjectile> ProjectileClass,
		EFE_ChargedProjectileAttachmentTarget AttachmentTarget,
		FName AttachSocketName,
		const FTransform& AttachOffset,
		bool bPredictedByOwner,
		float FullChargeSeconds,
		UAnimMontage* ChargeMontage,
		bool bBow);
	void StopChargeProjectilePresentation(bool bPredictedByOwner);

	/** Local-only camera presentation that survives the charging ability ending. */
	void StartChargeCameraPresentation(
		const FTransform& TargetRelativeTransform,
		float BlendInDuration,
		float BlendOutDuration);
	void StopChargeCameraPresentation();

	/** Owning-client presentation state consumed by BowCrossHairWidget. */
	void SetBowChargeAlpha(float NewChargeAlpha);
	float GetBowChargeAlpha() const;
	bool IsBowCharging() const;
	FOnFEBowChargeChanged& OnBowChargeChanged() { return BowChargeChangedDelegate; }

	/** Applies an animation-curve value to both equipped bow meshes without replication. */
	void SetBowVisualAlpha(float NewVisualAlpha);

	/** Applies and smoothly recovers owning-player control rotation. Never replicated or multicast. */
	void ApplyLocalWeaponRecoil(const FFE_RecoilSettings& RecoilSettings, float RecoilControl);

	/** Cancels montage/preview presentation before equipment data is replaced. */
	void StopWeaponActionPresentation(bool bPredictedByOwner);

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

	/** Runtime reactions supplied by the owning character's cached AI settings. */
	void SetHitReactionMontages(const TArray<TObjectPtr<UAnimMontage>>& NewMontages)
	{
		HitReactionMontages = NewMontages;
	}

	/** Plays the configured death montage once when this actor's Health reaches zero. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat|Death")
	void HandleDeath();

	/** Returns the ASC exposed by the actor, including PlayerState-owned ASCs. */
	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	static UAbilitySystemComponent* FindAbilitySystemComponent(AActor* Actor);
	static bool CanActorAttack(AActor* Actor);
	static bool IsActorAlive(AActor* Actor);

protected:
	/** Override this per character or weapon when a different damage GE is required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="FallenEra|Combat|Reaction")
	TArray<TObjectPtr<UAnimMontage>> HitReactionMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Death")
	TSoftObjectPtr<UAnimMontage> DeathMontage;

private:
	static constexpr float LocalPresentationUpdateInterval = 1.0f / 60.0f;
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttackMontage(UAnimMontage* Montage, bool bPredictedByOwner);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayWeaponMeshMontage(UAnimMontage* Montage, bool bPredictedByOwner);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayWeaponAttackEffects(UFE_WeaponItemData* WeaponData, int32 AttackIndex, bool bPredictedByOwner);
	void PlayWeaponDataEffectsLocal(const UFE_WeaponItemData* WeaponData, int32 AttackIndex);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStopWeaponActionPresentation(bool bPredictedByOwner);

	UPROPERTY(ReplicatedUsing=OnRep_ChargeState)
	FFE_ChargePresentationState ChargeState;
	FFE_ChargePresentationState LocalChargeState;
	UFUNCTION() void OnRep_ChargeState(const FFE_ChargePresentationState& PreviousState);
	void RestoreChargePresentation();
	void HandleWeaponChanged(const UFE_WeaponItemData* WeaponData);
	void HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount);
	void CancelAttackActions();
	void FinishLoadingCombatAssets();
	float GetPresentationServerTime() const;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
	FDelegateHandle DeathTagHandle;
	FDelegateHandle WeaponChangedHandle;
	TSharedPtr<FStreamableHandle> CombatAssetLoadHandle;
	TSharedPtr<FStreamableHandle> ChargeAssetLoadHandle;
	UPROPERTY(Transient) TSubclassOf<UGameplayEffect> CachedDamageEffectClass;

	void StartChargeProjectilePresentationLocal(
		TSubclassOf<AFE_CombatProjectile> ProjectileClass,
		EFE_ChargedProjectileAttachmentTarget AttachmentTarget,
		FName AttachSocketName,
		const FTransform& AttachOffset);
	void StopChargeProjectilePresentationLocal();
	void StopWeaponActionPresentationLocal();
	void StartCameraTransition(const FTransform& TargetTransform, float Duration, bool bReturning);
	void HandleCameraTransition();
	void HandleRecoilRecovery();
	void PlayWeaponAttackEffectsLocal(
		USoundBase* AttackSound,
		float SoundVolume,
		float SoundPitch,
		UNiagaraSystem* MuzzleSystem,
		UParticleSystem* MuzzleParticleSystem,
		FName MuzzleSocketName,
		const FVector& MuzzleScale);
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
	void ApplyDeathCollisionState();

	UFUNCTION()
	void OnRep_DeathCollisionDisabled();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHitReaction(UAnimMontage* HitMontage);

	UPROPERTY(Replicated)
	float DeathStartServerTime = 0.0f;

	FTimerHandle StunTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<AFE_CombatProjectile> ChargeProjectilePreview;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAttackMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveWeaponMeshMontage;

	UPROPERTY(Transient)
	float BowChargeAlpha = 0.0f;
	FOnFEBowChargeChanged BowChargeChangedDelegate;

	TWeakObjectPtr<UCameraComponent> ChargeCamera;
	FTransform OriginalCameraRelativeTransform;
	FTransform CameraTransitionStartTransform;
	FTransform CameraTransitionTargetTransform;
	FTimerHandle CameraTransitionTimer;
	FTimerHandle RecoilRecoveryTimer;
	float CameraTransitionStartTime = 0.0f;
	float CameraTransitionDuration = 0.0f;
	float CameraBlendOutDuration = 0.2f;
	bool bOriginalCameraTransformCached = false;
	bool bChargeCameraPresentationActive = false;
	bool bCameraTransitionReturning = false;
	FRotator PendingRecoilRecovery = FRotator::ZeroRotator;
	float RecoilPitchRecoverySpeed = 0.0f;
	float RecoilYawRecoverySpeed = 0.0f;
	float LastRecoilRecoveryTime = 0.0f;

	bool bReactionStunActive = false;
	bool bDeathMontagePlayed = false;

	UPROPERTY(ReplicatedUsing=OnRep_DeathCollisionDisabled)
	bool bDeathCollisionDisabled = false;
};
