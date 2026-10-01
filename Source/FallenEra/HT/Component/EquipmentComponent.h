#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "EquipmentComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UInputMappingContext;
class UMeshComponent;
class UFE_WeaponAttackData;
class UFE_WeaponItemData;
class UFE_ArmorItemData;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFEWeaponChanged, const UFE_WeaponItemData*);

/** Owns weapon selection, presentation, granted weapon abilities, and equip-time effects. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_EquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFE_EquipmentComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Call after initializing/replacing the owner's ASC actor info. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Equipment")
	void RefreshEquipment();

	/** Asynchronously loads and retains every test-inventory weapon and its soft dependencies. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Equipment|Loading")
	void PreloadWeaponAssets();

	UFUNCTION(BlueprintCallable, Category="FallenEra|Equipment")
	void CycleWeapon();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment")
	void EquipWeaponByItemTag(FGameplayTag ItemTag);

	UFUNCTION(BlueprintCallable, Category="FallenEra|Equipment")
	void UnequipWeapon();

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	const UFE_WeaponItemData* GetCurrentWeaponData() const { return CurrentWeaponData; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	const UFE_WeaponAttackData* GetCurrentAttackForInputTag(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	FGameplayTag GetCurrentItemTag() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	int32 GetCurrentItemTagIndex() const { return CurrentItemTagIndex; }

	UMeshComponent* GetEquippedWorldWeaponMesh() const { return EquippedWorldWeaponMesh; }
	UMeshComponent* GetEquippedFirstPersonWeaponMesh() const { return EquippedFirstPersonWeaponMesh; }

	FOnFEWeaponChanged& OnWeaponChanged() { return WeaponChangedDelegate; }

	/** Server only. Replaces just this armor's slot; does not alter weapon equipment. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment|Armor")
	bool EquipArmor(UFE_ArmorItemData* ArmorData);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment|Armor")
	bool UnequipArmor(FGameplayTag SlotTag);

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Armor")
	const UFE_ArmorItemData* GetEquippedArmor(FGameplayTag SlotTag) const;

	/** Configure on the enemy/player EquipmentComponent. Loaded once after ASC initialization. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Armor")
	TArray<TSoftObjectPtr<UFE_ArmorItemData>> DefaultArmorDataAssets;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Equipment|Armor")
	bool bInitialArmorApplied = false;

protected:
	/** Temporary test inventory. Replace with inventory data when that system is available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Test Inventory")
	TArray<FGameplayTag> TestItemTags;

	/** Weapon data candidates matched against TestItemTags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Test Inventory")
	TArray<TSoftObjectPtr<UFE_WeaponItemData>> WeaponDataAssets;

	/** Starts the full weapon working-set preload during BeginPlay to avoid first-equip asset IO stalls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Loading")
	bool bPreloadWeaponAssetsOnBeginPlay = true;

	/** World representation, hidden from the owning player. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Presentation")
	TObjectPtr<UMeshComponent> EquippedWorldWeaponMesh;

	/** First-person representation, visible only to the owning player. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment|Presentation")
	TObjectPtr<UMeshComponent> EquippedFirstPersonWeaponMesh;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentItemTagIndex, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|State")
	int32 CurrentItemTagIndex = INDEX_NONE;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|State")
	TObjectPtr<UFE_WeaponItemData> CurrentWeaponData;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> CurrentWeaponAnimLayerClass;

	UFUNCTION()
	void OnRep_CurrentItemTagIndex();

	UFUNCTION(Server, Reliable)
	void ServerCycleWeapon();

	UFUNCTION(Server, Reliable)
	void ServerUnequipWeapon();

	void EquipCurrentWeapon();
	void InterruptWeaponActions();
	void EnsureWeaponMeshComponents(bool bUseSkeletalMesh);
	void ClearWeaponVisuals();
	void ApplyWeaponVisuals();
	void UpdateWeaponAnimLayers(TSubclassOf<UAnimInstance> NewAnimLayerClass);
	void CacheWeaponAbilities();
	void ClearWeaponAbilities();
	void ApplyEquippedWeaponOffense();
	void ClearEquippedWeaponOffense();
	void FinishLoadingWeaponDataAssets();
	void FinishLoadingWeaponDependencies();
	void FinishPreloadingWeaponDataAssets();
	void ApplyLoadedWeapon(UFE_WeaponItemData* WeaponData);
	void RequestWeaponLoad(const TArray<FSoftObjectPath>& AssetPaths, bool bLoadingDataAssets);
	void GatherWeaponDependencyPaths(
		const UFE_WeaponItemData* WeaponData,
		TArray<FSoftObjectPath>& OutPaths,
		bool bIncludeLoadedAssets = false) const;
	UFE_WeaponItemData* FindWeaponDataForTag(FGameplayTag ItemTag) const;

	TArray<FGameplayAbilitySpecHandle> GrantedWeaponAbilityHandles;
	FActiveGameplayEffectHandle EquippedWeaponOffenseEffectHandle;
	TWeakObjectPtr<UAbilitySystemComponent> WeaponAbilitySystem;
	TSharedPtr<FStreamableHandle> WeaponLoadHandle;
	/** Kept alive for the component lifetime so preloaded soft assets are not garbage-collected. */
	TSharedPtr<FStreamableHandle> WeaponPreloadHandle;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponItemData> PendingWeaponData;
	FOnFEWeaponChanged WeaponChangedDelegate;

private:
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Equipment|Armor")
	TArray<TObjectPtr<UFE_ArmorItemData>> EquippedArmor;

	TMap<FGameplayTag, FActiveGameplayEffectHandle> ArmorEffectHandles;
	TWeakObjectPtr<UAbilitySystemComponent> ArmorAbilitySystem;
	TSharedPtr<FStreamableHandle> ArmorLoadHandle;
	bool bArmorLoadRequested = false;
	bool bArmorEndingPlay = false;

	void RefreshArmorEquipment();
	void FinishLoadingDefaultArmor();
	void ClearArmorEffects();
	FActiveGameplayEffectHandle ApplyArmorEffect(UFE_ArmorItemData* ArmorData);
};
