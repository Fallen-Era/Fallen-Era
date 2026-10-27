#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Combat/Condition/FECharacterConditionTypes.h"
#include "FEPlayerStatusViewModel.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;
class UFE_CharacterStatusComponent;
class UFE_EquipmentComponent;
class UFE_WeaponItemData;
class UFE_StaminaComponent;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE(FFE_OnStatusValuesChanged);

/** Event-driven model. Existing native widgets and BP MVVM bindings share the same values. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_PlayerStatusViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	void Initialize(APlayerController* PlayerController);
	void Shutdown();
	void RefreshSources();
	bool GetConditionDefinition(FGameplayTag Tag, FFE_CharacterConditionDefinition& Definition) const;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status") float Health = 0.0f;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status") float MaxHealth = 1.0f;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status") float Stamina = 0.0f;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status") float MaxStamina = 1.0f;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status") bool bStaminaExhausted = false;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status") TArray<FFE_ActiveCharacterCondition> Conditions;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status|Ammo") int32 LoadedAmmo = 0;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status|Ammo") int32 ReserveAmmo = 0;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status|Ammo") bool bUsesAmmo = false;
	FFE_OnStatusValuesChanged ValuesChanged;
	FFE_OnStatusValuesChanged ConditionsChanged;
	FFE_OnStatusValuesChanged AmmoChanged;
	FFE_OnStatusValuesChanged StaminaActionRejected;
private:
	void HandlePawnChanged(APawn* Pawn);
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleStaminaStateChanged(FGameplayTag Tag, int32 Count);
	void HandleStaminaActionRejected();
	void RefreshValues();
	void RefreshConditions();
	UFUNCTION()
	void RefreshAmmo();
	void HandleWeaponChanged(const UFE_WeaponItemData* Weapon);
	void UnbindSources();
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UFE_CharacterStatusComponent> StatusComponent;
	TWeakObjectPtr<UFE_EquipmentComponent> EquipmentComponent;
	TWeakObjectPtr<UFE_StaminaComponent> StaminaComponent;
	FDelegateHandle PawnChangedHandle;
	FDelegateHandle PlayerStateChangedHandle;
	FDelegateHandle ConditionsChangedHandle;
	FDelegateHandle WeaponChangedHandle;
	FDelegateHandle StaminaStateHandle;
	FDelegateHandle StaminaRejectedHandle;
	TArray<FDelegateHandle> AttributeChangedHandles;
};
