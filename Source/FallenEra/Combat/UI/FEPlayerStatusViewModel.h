#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Combat/Condition/FECharacterConditionTypes.h"
#include "FEPlayerStatusViewModel.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;
class UFE_CharacterStatusComponent;
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
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Player Status") TArray<FFE_ActiveCharacterCondition> Conditions;
	FFE_OnStatusValuesChanged ValuesChanged;
	FFE_OnStatusValuesChanged ConditionsChanged;
private:
	void HandlePawnChanged(APawn* Pawn);
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshValues();
	void RefreshConditions();
	void UnbindSources();
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UFE_CharacterStatusComponent> StatusComponent;
	FDelegateHandle PawnChangedHandle;
	FDelegateHandle PlayerStateChangedHandle;
	FDelegateHandle ConditionsChangedHandle;
	TArray<FDelegateHandle> AttributeChangedHandles;
};
