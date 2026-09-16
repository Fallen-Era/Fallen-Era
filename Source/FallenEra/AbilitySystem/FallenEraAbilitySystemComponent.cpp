#include "AbilitySystem/FallenEraAbilitySystemComponent.h"

#include "AbilitySystem/Abilities/FallenEraGameplayAbility.h"
#include "AbilitySystem/FallenEraGameplayTags.h"

void UFallenEraAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			// Do not discard a pending release while an ability is active. Otherwise
			// a quick re-press can keep an attack ability active indefinitely.
			if (!AbilitySpec.IsActive())
			{
				InputReleasedSpecHandles.Remove(AbilitySpec.Handle);
				PendingReactivationSpecHandles.Remove(AbilitySpec.Handle);
			}
			else
			{
				// Preserve a press that happened during the previous attack. It is
				// consumed once the active ability finishes.
				PendingReactivationSpecHandles.AddUnique(AbilitySpec.Handle);
			}
			InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
			InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
		}
	}
}

void UFallenEraAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
			InputHeldSpecHandles.Remove(AbilitySpec.Handle);
			PendingReactivationSpecHandles.Remove(AbilitySpec.Handle);
		}
	}
}

void UFallenEraAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	(void)DeltaTime;
	(void)bGamePaused;

	if (HasMatchingGameplayTag(FallenEraGameplayTags::State_InputBlocked))
	{
		ClearAbilityInput();
		return;
	}

	TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;

	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			const UFallenEraGameplayAbility* AbilityCDO = Cast<UFallenEraGameplayAbility>(AbilitySpec->Ability);
			if (AbilityCDO && AbilityCDO->GetActivationPolicy() == EFallenEraAbilityActivationPolicy::WhileInputActive && !AbilitySpec->IsActive())
			{
				AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpecInputPressed(*AbilitySpec);

				const UFallenEraGameplayAbility* AbilityCDO = Cast<UFallenEraGameplayAbility>(AbilitySpec->Ability);
				// Weapon data accepts regular UGameplayAbility classes as well. They use
				// the one-shot input policy unless they opt into a FallenEra policy.
				const bool bOnInputTriggered = !AbilityCDO ||
					AbilityCDO->GetActivationPolicy() == EFallenEraAbilityActivationPolicy::OnInputTriggered;
				if (bOnInputTriggered && !AbilitySpec->IsActive())
				{
					AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
				}
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(AbilitySpecHandle);
	}

	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpecInputReleased(*AbilitySpec);
			}
		}
	}

	// If the player pressed again while the previous attack was active, start
	// exactly one new activation after that ability has ended.
	for (int32 PendingIndex = PendingReactivationSpecHandles.Num() - 1; PendingIndex >= 0; --PendingIndex)
	{
		const FGameplayAbilitySpecHandle SpecHandle = PendingReactivationSpecHandles[PendingIndex];
		FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle);
		if (!AbilitySpec || !AbilitySpec->Ability)
		{
			PendingReactivationSpecHandles.RemoveAtSwap(PendingIndex);
			continue;
		}

		if (!InputHeldSpecHandles.Contains(SpecHandle))
		{
			PendingReactivationSpecHandles.RemoveAtSwap(PendingIndex);
			continue;
		}

		if (!AbilitySpec->IsActive())
		{
			TryActivateAbility(SpecHandle);
			PendingReactivationSpecHandles.RemoveAtSwap(PendingIndex);
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UFallenEraAbilitySystemComponent::ClearAbilityInput()
{
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle); AbilitySpec && AbilitySpec->InputPressed)
		{
			AbilitySpecInputReleased(*AbilitySpec);
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
	PendingReactivationSpecHandles.Reset();
}

void UFallenEraAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const UFallenEraGameplayAbility* AbilityCDO = Cast<UFallenEraGameplayAbility>(AbilitySpec.Ability);
		if (AbilityCDO && AbilityCDO->GetActivationPolicy() == EFallenEraAbilityActivationPolicy::OnSpawn && !AbilitySpec.IsActive())
		{
			TryActivateAbility(AbilitySpec.Handle);
		}
	}
}

void UFallenEraAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	if (Spec.IsActive())
	{
		const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
		if (!Instances.IsEmpty())
		{
			InvokeReplicatedEvent(
				EAbilityGenericReplicatedEvent::InputPressed,
				Spec.Handle,
				Instances.Last()->GetCurrentActivationInfoRef().GetActivationPredictionKey());
		}
	}
}

void UFallenEraAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (Spec.IsActive())
	{
		const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
		if (!Instances.IsEmpty())
		{
			InvokeReplicatedEvent(
				EAbilityGenericReplicatedEvent::InputReleased,
				Spec.Handle,
				Instances.Last()->GetCurrentActivationInfoRef().GetActivationPredictionKey());
		}
	}
}
