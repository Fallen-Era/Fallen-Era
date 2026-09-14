#include "FallenEraPlayerState.h"

#include "AbilitySystem/FallenEraAbilitySet.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"

AFallenEraPlayerState::AFallenEraPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UFallenEraAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UFallenEraAttributeSet>(TEXT("AttributeSet"));
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* AFallenEraPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AFallenEraPlayerState::InitializeAbilitySystem(AActor* AvatarActor)
{
	if (!AbilitySystemComponent || !AvatarActor)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, AvatarActor);

	if (HasAuthority() && !AbilitySystemComponent->HasMatchingGameplayTag(FallenEraGameplayTags::State_AbilitySystem_Initialized))
	{
		for (const UFallenEraAbilitySet* AbilitySet : DefaultAbilitySets)
		{
			if (AbilitySet)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this);
			}
		}

		AbilitySystemComponent->AddLooseGameplayTag(
			FallenEraGameplayTags::State_AbilitySystem_Initialized,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
		AbilitySystemComponent->TryActivateAbilitiesOnSpawn();
	}
}

bool AFallenEraPlayerState::HasStateTag(FGameplayTag StateTag) const
{
	return AbilitySystemComponent && StateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(StateTag);
}

void AFallenEraPlayerState::AddStateTag(FGameplayTag StateTag)
{
	if (HasAuthority() && AbilitySystemComponent && StateTag.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(StateTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void AFallenEraPlayerState::RemoveStateTag(FGameplayTag StateTag)
{
	if (HasAuthority() && AbilitySystemComponent && StateTag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(StateTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}
