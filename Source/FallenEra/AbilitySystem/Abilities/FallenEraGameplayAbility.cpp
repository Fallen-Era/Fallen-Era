#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"

#include "AbilitySystem/FallenEraGameplayTags.h"

UFallenEraGameplayAbility::UFallenEraGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(FallenEraGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(FallenEraGameplayTags::State_Stunned);
}
