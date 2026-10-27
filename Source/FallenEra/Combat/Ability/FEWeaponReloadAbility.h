#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"
#include "FEWeaponReloadAbility.generated.h"

class UAbilityTask_WaitDelay;
class UFE_EquipmentComponent;

/** Granted by equipment. Reserve transfer happens once, on successful server completion. */
UCLASS()
class FALLENERA_API UFE_WeaponReloadAbility : public UFallenEraGameplayAbility
{
	GENERATED_BODY()
public:
	UFE_WeaponReloadAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	UFUNCTION() void CompleteReload();
	UPROPERTY() TObjectPtr<UAbilityTask_WaitDelay> ReloadTask;
	TWeakObjectPtr<UFE_EquipmentComponent> Equipment;
	bool bStartedReload = false;
};
