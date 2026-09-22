#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "FallenEraCharacter.h"
#include "HT/Component/EquipmentComponent.h"
#include "HT/Interface/Damageable.h"
#include "HT/Interface/CombatPresentation.h"
#include "CombatCharacter.generated.h"

class UInputAction;
class UInputMappingContext;
class UAnimMontage;
class UFE_WeaponItemData;
class UFE_WeaponAttackData;

/**
 * Personal combat-character sandbox. Keep this class isolated until the weapon flow is ready to merge
 * into AFallenEraCharacter.
 */
UCLASS()
class FALLENERA_API AFE_CombatCharacter : public AFallenEraCharacter, public IFE_CombatPresentation, public IFE_Damageable
{
	GENERATED_BODY()

public:
	AFE_CombatCharacter();
	virtual USkeletalMeshComponent* GetCombatFirstPersonMesh() const override { return GetFirstPersonMesh(); }
	virtual FFE_CombatDamageResult ReceiveCombatDamage_Implementation(
		const FFE_CombatDamageRequest& DamageRequest) override;

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat|Weapon")
	void CycleWeapon();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="FallenEra|Combat|Weapon")
	void EquipWeaponByItemTag(FGameplayTag ItemTag);

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Weapon")
	const UFE_WeaponItemData* GetCurrentWeaponData() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Weapon")
	const UFE_WeaponAttackData* GetCurrentAttackForInputTag(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Weapon")
	FGameplayTag GetCurrentItemTag() const;

	FOnFEWeaponChanged& OnWeaponChanged() { return EquipmentComponent->OnWeaponChanged(); }

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Weapon")
	int32 GetCurrentItemTagIndex() const;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Equipment")
	UFE_EquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	/** Plays an attack montage on the character's world/first-person meshes. */
	void PlayAttackMontage(UAnimMontage* Montage);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttackMontage(UAnimMontage* Montage);

protected:
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Combat|Equipment", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UFE_EquipmentComponent> EquipmentComponent;

	void HandleCombatLeftClickStarted(const FInputActionValue& Value);
	void HandleCombatLeftClickReleased(const FInputActionValue& Value);
	void HandleCombatRightClickStarted(const FInputActionValue& Value);
	void HandleCombatRightClickReleased(const FInputActionValue& Value);
	void HandleCombatSwap(const FInputActionValue& Value);

	void SetCombatInputEnabled(bool bEnabled);
	void PressCombatAbility(const FGameplayTag& InputTag);
	void ReleaseCombatAbility(const FGameplayTag& InputTag);
};
