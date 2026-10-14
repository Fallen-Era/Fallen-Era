#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"
#include "FEWeaponAimAbility.generated.h"

class UFE_WeaponAimingComponent;

/** Equipment grants this once per aiming-enabled weapon, bound to RightClick. */
UCLASS()
class FALLENERA_API UFE_WeaponAimAbility : public UFallenEraGameplayAbility
{
	GENERATED_BODY()
public:
	UFE_WeaponAimAbility();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);
	TWeakObjectPtr<UFE_WeaponAimingComponent> Aiming;
};
