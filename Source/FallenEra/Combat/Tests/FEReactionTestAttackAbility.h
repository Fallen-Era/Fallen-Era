#pragma once

#include "Combat/Ability/FEPlayerAttackAbility.h"
#include "FEReactionTestAttackAbility.generated.h"

class UFE_MeleeTraceTask;

/** Native automation fixture: exercises the production cache/EndAbility lifecycle without animation assets. */
UCLASS(NotBlueprintable, Transient)
class UFE_ReactionTestAttackAbility : public UFE_PlayerAttackAbility
{
	GENERATED_BODY()

public:
	UFE_ReactionTestAttackAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	bool IsTraceActive() const;

private:
	UPROPERTY(Transient) TObjectPtr<UFE_MeleeTraceTask> TraceTask;
};
