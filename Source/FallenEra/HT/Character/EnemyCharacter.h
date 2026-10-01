#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayTagContainer.h"
#include "HT/AI/AISettings.h"
#include "HT/Interface/Damageable.h"
#include "HT/Interface/ConditionSource.h"
#include "EnemyCharacter.generated.h"

class UFE_CombatComponent;
class UFE_EquipmentComponent;
class UFE_EnemyMeleeAttackAbility;
class UFE_AISettingsDataAsset;
class UFallenEraAbilitySet;
class UFallenEraAbilitySystemComponent;
class UFallenEraAttributeSet;
class UAnimMontage;
class UBlendSpace;
class UGameplayEffect;
class UNavigationInvokerComponent;

/** Reusable ACharacter base for enemies with their own GAS owner and combat component. */
UCLASS(Abstract)
class FALLENERA_API AFE_EnemyCharacter : public ACharacter, public IAbilitySystemInterface,
	public IFE_Damageable, public IFE_ConditionSource, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AFE_EnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FFE_CombatDamageResult ReceiveCombatDamage_Implementation(
		const FFE_CombatDamageRequest& DamageRequest) override;
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual TArray<FFE_ConditionApplicationChance> GetConditionApplicationChances_Implementation() const override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	UFallenEraAbilitySystemComponent* GetEnemyAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	const UFallenEraAttributeSet* GetEnemyAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	UFE_CombatComponent* GetCombatComponent() const { return CombatComponent; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	UFE_EquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	bool IsDead() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|AI|Combat")
	bool TryStartAttack(AActor* TargetActor);

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Combat")
	float GetAttackRange() const { return CachedAISettings.AttackRange; }

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Combat")
	bool IsAttackInProgress() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|AI|Combat")
	void CancelActiveAttack();

	AActor* GetPendingAttackTarget() const { return PendingAttackTarget.Get(); }
	void ClearPendingAttackTarget();
	const TArray<FSAIAttackMontageSettings>& GetAttackMontageSettings() const
	{
		return CachedAISettings.AttackMontages;
	}
	float GetAttackCooldown() const { return CachedAISettings.AttackCooldown; }
	TSubclassOf<UGameplayEffect> GetCachedAttackDamageEffect() const { return CachedAttackDamageEffect; }
	float GetPatrolMovementSpeed() const { return CachedAISettings.PatrolMovementSpeed; }
	float GetChaseMovementSpeed() const { return CachedAISettings.ChaseMovementSpeed; }
	const FSAITargetSelectionSettings& GetTargetSelectionSettings() const
	{
		return CachedAISettings.TargetSelection;
	}

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Settings")
	FSAISettings GetCachedAISettings() const { return CachedAISettings; }

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Settings")
	UBlendSpace* GetCachedLocomotionBlendSpace() const
	{
		return CachedAISettings.LocomotionBlendSpace;
	}

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Settings")
	bool IsAISettingsCached() const { return bAISettingsCached; }

	/** Called before deferred spawning finishes so the AI controller uses centralized distance management. */
	void ConfigureSpawnManagement(float VisualCullDistance, float NetCullDistance);
	void SetManagedSimulationActive(bool bActive);
	bool IsSpawnManaged() const { return bManagedBySpawnSubsystem; }
	bool IsManagedSimulationActive() const { return bManagedSimulationActive; }

	void SetNavigationInvokerActive(bool bActive);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Abilities")
	TObjectPtr<UFallenEraAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Abilities")
	TObjectPtr<UFallenEraAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Combat")
	TObjectPtr<UFE_CombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Equipment")
	TObjectPtr<UFE_EquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|AI|Navigation")
	TObjectPtr<UNavigationInvokerComponent> NavigationInvokerComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Abilities")
	TArray<TObjectPtr<UFallenEraAbilitySet>> DefaultAbilitySets;

	/** Server-only, per-enemy attack ability. Granted automatically when it is not already in an ability set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Abilities")
	TSubclassOf<UFE_EnemyMeleeAttackAbility> AttackAbilityClass;

	/** Available AI presets. All runtime systems read the selected preset's cached copy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|AI|Settings")
	TArray<TSoftObjectPtr<UFE_AISettingsDataAsset>> AISettingsDataAssets;

	/** When enabled, the server randomly selects one valid preset for each enemy at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|AI|Settings")
	bool bRandomizeAISettings = false;

	/** Used when random selection is disabled. Also replicated as the resolved runtime selection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_SelectedAISettingsIndex,
		Category="FallenEra|AI|Settings", meta=(ClampMin="0", EditCondition="!bRandomizeAISettings"))
	int32 SelectedAISettingsIndex = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="FallenEra|AI|Settings")
	FSAISettings CachedAISettings;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="FallenEra|AI|Settings")
	TObjectPtr<UFE_AISettingsDataAsset> CachedAISettingsDataAsset;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="FallenEra|AI|Settings")
	bool bAISettingsCached = false;

	UPROPERTY(ReplicatedUsing=OnRep_ManagedVisualCullDistance, Transient)
	float ManagedVisualCullDistance = 0.0f;

private:
	void SelectAISettingsIndexAtBeginPlay();
	void CacheSelectedAISettings();
	void InitializeEnemyAbilitySystem();

	UFUNCTION()
	void OnRep_SelectedAISettingsIndex();

	UFUNCTION()
	void OnRep_ManagedVisualCullDistance();

	void ApplyManagedVisualCullDistance();

	TWeakObjectPtr<AActor> PendingAttackTarget;
	TSubclassOf<UGameplayEffect> CachedAttackDamageEffect;
	bool bManagedBySpawnSubsystem = false;
	bool bManagedSimulationActive = true;
};
