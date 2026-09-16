#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "FallenEraCharacter.h"
#include "CombatCharacter.generated.h"

class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;
class UAnimInstance;
class UFE_WeaponItemData;
class UFE_WeaponAttackData;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFEWeaponChanged, const UFE_WeaponItemData*);

/**
 * Personal combat-character sandbox. Keep this class isolated until the weapon flow is ready to merge
 * into AFallenEraCharacter.
 */
UCLASS()
class FALLENERA_API AFE_CombatCharacter : public AFallenEraCharacter
{
	GENERATED_BODY()

public:
	AFE_CombatCharacter();

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat|Weapon")
	void CycleWeapon();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Combat|Weapon")
	void EquipWeaponByItemTag(FGameplayTag ItemTag);

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Weapon")
	const UFE_WeaponItemData* GetCurrentWeaponData() const { return CurrentWeaponData; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Weapon")
	const UFE_WeaponAttackData* GetCurrentAttackForInputTag(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Weapon")
	FGameplayTag GetCurrentItemTag() const;

	FOnFEWeaponChanged& OnWeaponChanged() { return WeaponChangedDelegate; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Weapon")
	int32 GetCurrentItemTagIndex() const { return CurrentItemTagIndex; }

protected:
	/** Temporary test inventory. Inventory equipment will replace this input later. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|Combat|Test Inventory")
	TArray<FGameplayTag> TestItemTags;

	/** Weapon data candidates searched by TestItemTags. Use soft references to keep the character light. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|Combat|Test Inventory")
	TArray<TSoftObjectPtr<UFE_WeaponItemData>> WeaponDataAssets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	int32 CombatMappingPriority = 20;

	/** Create these assets as IA_Combat_LeftClick, IA_Combat_RightClick, and IA_Combat_Swap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	TObjectPtr<UInputAction> CombatLeftClickAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	TObjectPtr<UInputAction> CombatRightClickAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	TObjectPtr<UInputAction> CombatSwapAction;

	/** World representation: other players see this on the full-body mesh, while the owner does not. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Combat|Weapon")
	TObjectPtr<UStaticMeshComponent> EquippedWorldWeaponMesh;

	/** First-person representation: only the owning player sees this on FirstPersonMesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Combat|Weapon")
	TObjectPtr<UStaticMeshComponent> EquippedFirstPersonWeaponMesh;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentItemTagIndex, VisibleInstanceOnly, BlueprintReadOnly, Category="FallenEra|Combat|Weapon")
	int32 CurrentItemTagIndex = INDEX_NONE;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="FallenEra|Combat|Weapon")
	TObjectPtr<UFE_WeaponItemData> CurrentWeaponData;

	/** The currently linked weapon layer is tracked so a swap can unlink it from both meshes first. */
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> CurrentWeaponAnimLayerClass;

	UFUNCTION()
	void OnRep_CurrentItemTagIndex();

	UFUNCTION(Server, Reliable)
	void ServerCycleWeapon();

	void HandleCombatLeftClickStarted(const FInputActionValue& Value);
	void HandleCombatLeftClickReleased(const FInputActionValue& Value);
	void HandleCombatRightClickStarted(const FInputActionValue& Value);
	void HandleCombatRightClickReleased(const FInputActionValue& Value);
	void HandleCombatSwap(const FInputActionValue& Value);

	void SetCombatInputEnabled(bool bEnabled);
	void EquipCurrentWeapon();
	void ClearWeaponVisuals();
	void ApplyWeaponVisuals();
	void UpdateWeaponAnimLayers(TSubclassOf<UAnimInstance> NewAnimLayerClass);
	void CacheWeaponAbilities();
	void ClearWeaponAbilities();
	UFE_WeaponItemData* FindWeaponDataForTag(FGameplayTag ItemTag) const;
	void PressCombatAbility(const FGameplayTag& InputTag);
	void ReleaseCombatAbility(const FGameplayTag& InputTag);

	TArray<FGameplayAbilitySpecHandle> GrantedWeaponAbilityHandles;
	FOnFEWeaponChanged WeaponChangedDelegate;
};
