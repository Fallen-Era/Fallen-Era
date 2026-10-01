#include "Combat/Component/CharacterStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/CombatGameplayTags.h"
#include "Combat/GameplayEffect/ConditionHealthLossGameplayEffect.h"
#include "Combat/Interface/ConditionSource.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UFE_CharacterStatusComponent::UFE_CharacterStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	BleedingCondition.ConditionTag = FallenEraCombatGameplayTags::State_Condition_Bleeding;
	BleedingCondition.DisplayName = NSLOCTEXT("CharacterCondition", "Bleeding", "Bleeding");
	BleedingCondition.Duration = 10.0f;
	BleedingCondition.TickInterval = 2.0f;
	BleedingCondition.HealthLossPerTick = 5.0f;

	InfectionCondition.ConditionTag = FallenEraCombatGameplayTags::State_Condition_Infection;
	InfectionCondition.DisplayName = NSLOCTEXT("CharacterCondition", "Infection", "Infection");
	InfectionCondition.Duration = 60.0f;
	InfectionCondition.ExpirationBehavior = EFE_ConditionExpirationBehavior::KillOwner;
}

void UFE_CharacterStatusComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UFE_CharacterStatusComponent, ActiveConditions, COND_OwnerOnly);
}

void UFE_CharacterStatusComponent::TryApplyConditionsFromDamageSource(AActor* DamageSource)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !DamageSource ||
		!DamageSource->GetClass()->ImplementsInterface(UFE_ConditionSource::StaticClass()))
	{
		return;
	}

	const TArray<FFE_ConditionApplicationChance> ApplicationChances =
		IFE_ConditionSource::Execute_GetConditionApplicationChances(DamageSource);
	for (const FFE_ConditionApplicationChance& Application : ApplicationChances)
	{
		const float ClampedChance = FMath::Clamp(Application.Chance, 0.0f, 1.0f);
		if (Application.ConditionTag.IsValid() && FindConditionDefinition(Application.ConditionTag) &&
			ClampedChance > 0.0f && (ClampedChance >= 1.0f || FMath::FRand() < ClampedChance))
		{
			ApplyCondition(Application.ConditionTag);
		}
	}
}

bool UFE_CharacterStatusComponent::ApplyCondition(FGameplayTag ConditionTag)
{
	AActor* OwnerActor = GetOwner();
	const FFE_CharacterConditionDefinition* Definition = FindConditionDefinition(ConditionTag);
	if (!OwnerActor || !OwnerActor->HasAuthority() || !Definition ||
		!ConditionTag.IsValid() ||
		(HasCondition(ConditionTag) && !Definition->bRefreshDurationOnReapply))
	{
		return false;
	}

	if (HasCondition(ConditionTag))
	{
		RemoveCondition(ConditionTag);
	}
	StartCondition(*Definition);
	return true;
}

bool UFE_CharacterStatusComponent::RemoveCondition(FGameplayTag ConditionTag)
{
	AActor* OwnerActor = GetOwner();
	FActiveConditionRuntime* Runtime = ActiveConditionRuntime.Find(ConditionTag);
	if (!OwnerActor || !OwnerActor->HasAuthority() || !Runtime)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Runtime->TickTimer);
		World->GetTimerManager().ClearTimer(Runtime->ExpirationTimer);
	}

	if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor))
	{
		if (Runtime->AppliedEffectHandle.IsValid())
		{
			AbilitySystem->RemoveActiveGameplayEffect(Runtime->AppliedEffectHandle);
		}
		AbilitySystem->RemoveLooseGameplayTag(
			ConditionTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}

	ActiveConditionRuntime.Remove(ConditionTag);
	ActiveConditions.RemoveAll(
		[ConditionTag](const FFE_ActiveCharacterCondition& ActiveCondition)
		{
			return ActiveCondition.ConditionTag == ConditionTag;
		});
	NotifyConditionsChanged();
	return true;
}

void UFE_CharacterStatusComponent::RemoveAllConditions()
{
	const TArray<FGameplayTag> TagsToRemove = GetActiveConditionTags();
	for (const FGameplayTag& ConditionTag : TagsToRemove)
	{
		RemoveCondition(ConditionTag);
	}
}

bool UFE_CharacterStatusComponent::HasCondition(FGameplayTag ConditionTag) const
{
	return ActiveConditions.ContainsByPredicate(
		[ConditionTag](const FFE_ActiveCharacterCondition& ActiveCondition)
		{
			return ActiveCondition.ConditionTag == ConditionTag;
		});
}

TArray<FGameplayTag> UFE_CharacterStatusComponent::GetActiveConditionTags() const
{
	TArray<FGameplayTag> Result;
	Result.Reserve(ActiveConditions.Num());
	for (const FFE_ActiveCharacterCondition& ActiveCondition : ActiveConditions)
	{
		Result.Add(ActiveCondition.ConditionTag);
	}
	return Result;
}

bool UFE_CharacterStatusComponent::GetConditionDefinition(
	FGameplayTag ConditionTag,
	FFE_CharacterConditionDefinition& OutDefinition) const
{
	if (const FFE_CharacterConditionDefinition* Definition = FindConditionDefinition(ConditionTag))
	{
		OutDefinition = *Definition;
		return true;
	}
	return false;
}

void UFE_CharacterStatusComponent::OnRep_ActiveConditions()
{
	NotifyConditionsChanged();
}

const FFE_CharacterConditionDefinition* UFE_CharacterStatusComponent::FindConditionDefinition(
	FGameplayTag ConditionTag) const
{
	if (BleedingCondition.ConditionTag == ConditionTag)
	{
		return &BleedingCondition;
	}
	if (InfectionCondition.ConditionTag == ConditionTag)
	{
		return &InfectionCondition;
	}
	return AdditionalConditions.FindByPredicate(
		[ConditionTag](const FFE_CharacterConditionDefinition& Definition)
		{
			return Definition.ConditionTag == ConditionTag;
		});
}

void UFE_CharacterStatusComponent::StartCondition(const FFE_CharacterConditionDefinition& Definition)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	FActiveConditionRuntime& Runtime = ActiveConditionRuntime.Add(Definition.ConditionTag);
	FFE_ActiveCharacterCondition& ActiveCondition = ActiveConditions.AddDefaulted_GetRef();
	ActiveCondition.ConditionTag = Definition.ConditionTag;
	ActiveCondition.EndServerWorldTime = Definition.Duration > 0.0f
		? GetServerWorldTime() + Definition.Duration
		: 0.0;

	if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor))
	{
		AbilitySystem->AddLooseGameplayTag(
			Definition.ConditionTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
		if (Definition.AppliedEffectClass)
		{
			const FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
				Definition.AppliedEffectClass, 1.0f, Context);
			if (Spec.IsValid())
			{
				Runtime.AppliedEffectHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (Definition.TickInterval > 0.0f && Definition.HealthLossPerTick > 0.0f)
		{
			FTimerDelegate TickDelegate;
			TickDelegate.BindUObject(this, &UFE_CharacterStatusComponent::HandleConditionTick, Definition.ConditionTag);
			TimerManager.SetTimer(Runtime.TickTimer, TickDelegate, Definition.TickInterval, true);
		}
		if (Definition.Duration > 0.0f)
		{
			FTimerDelegate ExpirationDelegate;
			ExpirationDelegate.BindUObject(this, &UFE_CharacterStatusComponent::HandleConditionExpired, Definition.ConditionTag);
			TimerManager.SetTimer(Runtime.ExpirationTimer, ExpirationDelegate, Definition.Duration, false);
		}
	}

	NotifyConditionsChanged();
}

void UFE_CharacterStatusComponent::HandleConditionTick(FGameplayTag ConditionTag)
{
	if (const FFE_CharacterConditionDefinition* Definition = FindConditionDefinition(ConditionTag))
	{
		ApplyFixedHealthLoss(Definition->HealthLossPerTick);
	}
}

void UFE_CharacterStatusComponent::HandleConditionExpired(FGameplayTag ConditionTag)
{
	const FFE_CharacterConditionDefinition* Definition = FindConditionDefinition(ConditionTag);
	const bool bKillOwner = Definition &&
		Definition->ExpirationBehavior == EFE_ConditionExpirationBehavior::KillOwner;
	RemoveCondition(ConditionTag);

	if (bKillOwner)
	{
		AActor* OwnerActor = GetOwner();
		UAbilitySystemComponent* AbilitySystem = OwnerActor
			? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor)
			: nullptr;
		if (AbilitySystem && !AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead))
		{
			ApplyFixedHealthLoss(AbilitySystem->GetNumericAttribute(UFallenEraAttributeSet::GetHealthAttribute()));
		}
	}
}

void UFE_CharacterStatusComponent::ApplyFixedHealthLoss(float HealthLoss)
{
	AActor* OwnerActor = GetOwner();
	UAbilitySystemComponent* AbilitySystem = OwnerActor
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor)
		: nullptr;
	if (!AbilitySystem || HealthLoss <= 0.0f ||
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead))
	{
		return;
	}

	const FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
		UFE_ConditionHealthLossGameplayEffect::StaticClass(), 1.0f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(
			FallenEraCombatGameplayTags::SetByCaller_Condition_HealthLoss,
			-FMath::Abs(HealthLoss));
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UFE_CharacterStatusComponent::NotifyConditionsChanged()
{
	ActiveConditionsChanged.Broadcast();
}

double UFE_CharacterStatusComponent::GetServerWorldTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
