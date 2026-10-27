#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FallenEraGameplayAbility.generated.h"

UENUM(BlueprintType)
enum class EFallenEraAbilityActivationPolicy : uint8
{
	OnInputTriggered,
	WhileInputActive,
	OnSpawn
};

/** Base ability used by the tag-driven input and automatic activation pipeline. */
UCLASS(Abstract, Blueprintable)
class FALLENERA_API UFallenEraGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UFallenEraGameplayAbility();

	EFallenEraAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	const FGameplayTagContainer& GetInputRetryBlockedTags() const { return InputRetryBlockedTags; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability|Activation")
	EFallenEraAbilityActivationPolicy ActivationPolicy = EFallenEraAbilityActivationPolicy::OnInputTriggered;

	/** A held press blocked by these states retries once successfully, or is discarded on release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ability|Activation")
	FGameplayTagContainer InputRetryBlockedTags;
};
