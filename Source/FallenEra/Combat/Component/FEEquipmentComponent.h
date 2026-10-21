#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "Combat/Weapon/Attachment/FEWeaponAttachmentData.h"
#include "Combat/Item/FEEquipmentInstanceData.h"
#include "Combat/Item/FEWeaponAmmoData.h"
#include "FEEquipmentComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UInputMappingContext;
class UMeshComponent;
class UFE_WeaponAttackData;
class UFE_WeaponItemData;
class UFE_ArmorItemData;
class UFE_WeaponSightData;
class UFE_AmmoRadialWidget;
class UStaticMeshComponent;
class USkeletalMeshComponent;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFEWeaponChanged, const UFE_WeaponItemData*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFEAmmoChanged);

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

	/** Enemy presets/initial attribute GEs must finish before component BeginPlay can apply armor. */
	void SetInitialArmorInitializationDeferred(bool bDeferred) { bInitialArmorInitializationDeferred = bDeferred; }

	/** Loads catalog metadata; only current/next weapon dependencies stay warm. */
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

	/** Server integration boundary for generated item data. Does not mutate the shared DA. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment|Instance")
	bool SetWeaponInstanceData(const FFE_WeaponInstanceData& NewInstance);

	/** Server snapshot; client combat stats continue to come from replicated GAS attributes. */
	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Instance")
	FFE_WeaponInstanceData GetWeaponInstanceData() const { return CurrentWeaponInstance; }
	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Stamina")
	float GetWeaponStaminaCost() const { return CurrentWeaponInstance.StaminaCost; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Ammo")
	FFE_WeaponAmmoState GetAmmoState() const { return AmmoState; }
	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Ammo")
	int32 GetReserveAmmo(FGameplayTag AmmoType) const;
	/** Server-only integration point for a future inventory. Replaces a loose-ammo stack. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment|Ammo")
	bool SetReserveAmmo(FGameplayTag AmmoType, int32 Count);
	UFUNCTION(BlueprintCallable, Category="FallenEra|Equipment|Ammo")
	bool RequestReload();
	/** Selects one server-owned stack, never accepts client-provided damage/counts. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Equipment|Ammo")
	bool RequestReloadAmmo(FGuid AmmoId, FGuid WeaponInstanceId);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment|Ammo")
	bool AddAmmoInstance(const FFE_AmmoStack& Stack);
	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Ammo")
	TArray<FFE_AmmoStack> GetCompatibleAmmo() const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Ammo Selection", meta=(ClampMin="0.1", Units="s"))
	float AmmoSelectionHoldDuration = 0.25f;
	/** WBP parent must be FE_AmmoRadialWidget; the class and sector WBP are preloaded asynchronously. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Ammo Selection")
	TSoftClassPtr<UFE_AmmoRadialWidget> AmmoSelectionWidgetClass;
	void PreloadAmmoSelectionUI();
	UClass* GetAmmoSelectionWidgetClass() const { return LoadedAmmoOptionWidgetClass ? LoadedAmmoSelectionWidgetClass.Get() : nullptr; }
	FSimpleMulticastDelegate AmmoSelectionUIReady;
	UPROPERTY(BlueprintAssignable, Category="FallenEra|Equipment|Ammo")
	FOnFEAmmoChanged OnAmmoChanged;
	bool CanFireWeaponAmmo(const UFE_WeaponItemData* Weapon) const;
	bool ConsumeWeaponAmmo(const UFE_WeaponItemData* Weapon);
	bool ConsumeWeaponAmmo(const UFE_WeaponItemData* Weapon, FFE_AmmoRoundData& OutRound);
	void RefundWeaponAmmo(const UFE_WeaponItemData* Weapon, FGuid InstanceId);
	void RefundWeaponAmmo(const UFE_WeaponItemData* Weapon, FGuid InstanceId, const FFE_AmmoRoundData& Round);
	bool CanReloadWeapon() const;
	bool BeginWeaponReload();
	bool FinishWeaponReload();
	void CancelWeaponReload();
	void CompleteAmmoAttack(const UFE_WeaponItemData* Weapon, bool bCharged, bool bShotConsumed);

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	const UFE_WeaponAttackData* GetCurrentAttackForInputTag(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	FGameplayTag GetCurrentItemTag() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment")
	int32 GetCurrentItemTagIndex() const { return CurrentItemTagIndex; }

	UMeshComponent* GetEquippedWorldWeaponMesh() const { return EquippedWorldWeaponMesh; }
	UMeshComponent* GetEquippedFirstPersonWeaponMesh() const { return EquippedFirstPersonWeaponMesh; }
	/** Updates owner visibility only; never reloads items or regrants abilities. */
	void RefreshPerspectivePresentation();

	FOnFEWeaponChanged& OnWeaponChanged() { return WeaponChangedDelegate; }
	FOnFEWeaponChanged& OnWeaponAttachmentsChanged() { return WeaponAttachmentsChangedDelegate; }

	/** Server inventory/item-instance entry point. Replaces parts of the currently equipped firearm. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment|Attachments")
	bool SetWeaponAttachments(const TArray<FFE_WeaponAttachmentInstance>& Attachments);
	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Attachments")
	const UFE_WeaponSightData* GetActiveSightData() const { return ActiveSightData; }
	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Attachments")
	bool AreWeaponAttachmentsReady() const { return bWeaponAttachmentsReady; }
	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Attachments")
	TArray<FFE_WeaponAttachmentInstance> GetWeaponAttachments() const { return WeaponAttachmentLoadout.Attachments; }
	UMeshComponent* GetFirstPersonSightMesh() const;

	/** Server only. Replaces just this armor's slot; does not alter weapon equipment. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment|Armor")
	bool EquipArmor(UFE_ArmorItemData* ArmorData);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Equipment|Armor")
	bool EquipArmorInstance(UFE_ArmorItemData* ArmorData, const FFE_ArmorInstanceData& NewInstance);

	UFUNCTION(BlueprintPure, Category="FallenEra|Equipment|Armor")
	FFE_ArmorInstanceData GetArmorInstanceData(FGameplayTag SlotTag) const;

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
	/** Seeded once on authority. Only identical definitions AND instance stats can combine. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Test Inventory")
	TArray<FFE_AmmoStack> TestAmmoInventory;
	/** Temporary test inventory. Replace with inventory data when that system is available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Test Inventory")
	TArray<FGameplayTag> TestItemTags;

	/** Weapon data candidates matched against TestItemTags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Test Inventory")
	TArray<TSoftObjectPtr<UFE_WeaponItemData>> WeaponDataAssets;

	/** Preloads metadata and a bounded current/next weapon working set. */
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

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerCycleWeapon();

	UFUNCTION(Server, Reliable, WithValidation)
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
	void ApplyEquippedWeaponTags();
	void ClearEquippedWeaponTags();
	void FinishLoadingWeaponDataAssets();
	void FinishLoadingWeaponDependencies();
	void FinishPreloadingWeaponDataAssets();
	void FinishWarmingWeaponDataAssets();
	TMap<FGameplayTag, TSoftObjectPtr<UFE_WeaponItemData>> WeaponAssetByTag;
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
	// Separate ownership: ClearWeaponAbilities resets WeaponAbilitySystem, and the old ASC can outlive this pawn.
	TWeakObjectPtr<UAbilitySystemComponent> EquippedTagsAbilitySystem;
	FGameplayTagContainer AppliedEquippedTags;
	TSharedPtr<FStreamableHandle> WeaponLoadHandle;
	/** Temporary catalog scan handle. Released once ItemTag -> asset paths have been cached. */
	TSharedPtr<FStreamableHandle> WeaponPreloadHandle;
	TSharedPtr<FStreamableHandle> WarmWeaponLoadHandle;
	bool CanChangeEquipment() const;
	float LastEquipmentChangeTime = -BIG_NUMBER;
	UPROPERTY(EditDefaultsOnly, Category="Equipment|Networking", meta=(ClampMin="0.0", Units="s"))
	float EquipmentChangeInterval = 0.1f;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponItemData> PendingWeaponData;
	FOnFEWeaponChanged WeaponChangedDelegate;

private:
	UPROPERTY(ReplicatedUsing=OnRep_AmmoState)
	FFE_WeaponAmmoState AmmoState;
	UPROPERTY(ReplicatedUsing=OnRep_AmmoState, Transient)
	TArray<FFE_AmmoStack> AmmoInventory;
	UPROPERTY(Transient)
	TMap<FGameplayTag, FFE_WeaponInstanceData> TestLoadedAmmoByItemTag;
	FGuid PendingReloadAmmoId;
	FGuid ReloadWeaponInstanceId;
	FGuid RequestedReloadAmmoId;
	TSharedPtr<FStreamableHandle> AmmoLoadHandle;
	TSharedPtr<FStreamableHandle> AmmoUIHandle;
	UPROPERTY(Transient) TObjectPtr<UClass> LoadedAmmoSelectionWidgetClass;
	UPROPERTY(Transient) TObjectPtr<UClass> LoadedAmmoOptionWidgetClass;
	void FinishLoadingAmmoSelectionUI();
	UPROPERTY(Transient) TArray<TObjectPtr<UFE_AmmoItemData>> LoadedAmmoDefinitions;
	void FinishLoadingAmmoInventory();
	const FFE_AmmoStack* FindSelectedAmmo() const;
	bool StoreAmmoStack(const FFE_AmmoStack& Stack);
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestReloadAmmo(FGuid AmmoId, FGuid WeaponInstanceId);
	FGameplayTag AmmoInstanceItemTag;
	bool bAmmoInventoryInitialized = false;
	void InitializeAmmoInventory();
	void InitializeWeaponAmmo();
	void SaveLoadedWeaponAmmo();
	void PublishAmmoState();
	UFUNCTION() void OnRep_AmmoState();
	UPROPERTY(Transient)
	FFE_WeaponInstanceData CurrentWeaponInstance;
	/** Retains only the active definition while asynchronous refresh temporarily clears CurrentWeaponData. */
	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponItemData> WeaponInstanceTemplate;
	UPROPERTY(Transient)
	TMap<FGameplayTag, FFE_ArmorInstanceData> ArmorInstances;
	TWeakObjectPtr<USkeletalMeshComponent> DefaultFirstPersonMesh;
	FTransform UnequippedFirstPersonMeshTransform;
	void RestoreFirstPersonMeshTransform();

	UPROPERTY(ReplicatedUsing=OnRep_WeaponAttachments)
	FFE_WeaponAttachmentLoadout WeaponAttachmentLoadout;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFE_WeaponAttachmentData>> LoadedAttachmentDefinitions;
	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponSightData> ActiveSightData;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> WeaponAttachmentMeshes;
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> FirstPersonSightMesh;
	TSharedPtr<FStreamableHandle> AttachmentLoadHandle;
	FActiveGameplayEffectHandle AttachmentStatsEffectHandle;
	TWeakObjectPtr<UAbilitySystemComponent> AttachmentAbilitySystem;
	FOnFEWeaponChanged WeaponAttachmentsChangedDelegate;
	bool bWeaponAttachmentsReady = false;
	bool bLoadingAttachmentDependencies = false;
	UFUNCTION()
	void OnRep_WeaponAttachments();
	void InitializeWeaponAttachments();
	void RefreshWeaponAttachments();
	void FinishLoadingWeaponAttachments();
	void ClearWeaponAttachments();
	void ApplyWeaponAttachmentVisuals();
	void ApplyWeaponAttachmentStats();

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Equipment|Armor")
	TArray<TObjectPtr<UFE_ArmorItemData>> EquippedArmor;

	TMap<FGameplayTag, FActiveGameplayEffectHandle> ArmorEffectHandles;
	TWeakObjectPtr<UAbilitySystemComponent> ArmorAbilitySystem;
	TSharedPtr<FStreamableHandle> ArmorLoadHandle;
	bool bArmorLoadRequested = false;
	bool bArmorEndingPlay = false;
	bool bInitialArmorInitializationDeferred = false;

	void RefreshArmorEquipment();
	void FinishLoadingDefaultArmor();
	void ClearArmorEffects();
	FActiveGameplayEffectHandle ApplyArmorEffect(UFE_ArmorItemData* ArmorData, const FFE_ArmorInstanceData& ArmorInstance);
};
