#include "AbilitySystem/FallenEraAbilitySet.h"

#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"
#include "GameplayEffect.h"

void UFallenEraAbilitySet::GiveToAbilitySystem(UFallenEraAbilitySystemComponent* AbilitySystemComponent, UObject* SourceObject) const
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FFallenEraAbilitySet_Ability& AbilityToGrant : GrantedAbilities)
	{
		if (!AbilityToGrant.Ability)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityToGrant.Ability, AbilityToGrant.AbilityLevel, INDEX_NONE, SourceObject);
		if (AbilityToGrant.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);
		}
		AbilitySystemComponent->GiveAbility(AbilitySpec);
	}

	for (const FFallenEraAbilitySet_Effect& EffectToGrant : GrantedEffects)
	{
		if (!EffectToGrant.GameplayEffect)
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(SourceObject);
		const FGameplayEffectSpecHandle EffectSpec = AbilitySystemComponent->MakeOutgoingSpec(
			EffectToGrant.GameplayEffect, EffectToGrant.EffectLevel, EffectContext);
		if (EffectSpec.IsValid())
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
		}
	}
}
