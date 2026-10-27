#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"
#include "Combat/UI/FEAmmoSelectionInput.h"
#include "FEAmmoSelectionAbility.generated.h"

class UAbilityTask_WaitDelay;
class UFE_EquipmentComponent;
class UFE_AmmoRadialWidget;
class UFE_AmmoSelectionViewModel;

/** Local input/presentation only. Actual transfer remains in the ServerOnly reload GA. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_AmmoSelectionAbility : public UFallenEraGameplayAbility
{
	GENERATED_BODY()
public:
	UFE_AmmoSelectionAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	UFUNCTION() void OpenWheel();
	UFUNCTION() void OnInputReleased(float TimeHeld);
	UFUNCTION() void HandleAmmoChanged();
	UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
	void CancelSelection();
	void HandleApplicationActivation(bool bActive);
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitDelay> HoldTask;
	UPROPERTY(Transient) TObjectPtr<UFE_AmmoRadialWidget> Wheel;
	UPROPERTY(Transient) TObjectPtr<UFE_AmmoSelectionViewModel> Model;
	TWeakObjectPtr<UFE_EquipmentComponent> Equipment;
	TWeakObjectPtr<APlayerController> InputOwner;
	FFE_AmmoSelectionInput InputState;
	FGuid WeaponInstanceId;
	FDelegateHandle ActivationHandle;
	FDelegateHandle UIReadyHandle;
	bool bWheelOpened = false;
	bool bHoldElapsed = false;
	bool bOpeningWheel = false;
	bool bSelectingTagAdded = false;
};
