#include "Combat/Component/FECharacterStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/GameplayEffect/FEConditionHealthLossGameplayEffect.h"
#include "Combat/Interface/FEConditionSource.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Combat/GameplayEffect/FEConditionLifetimeGameplayEffect.h"

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
		!ConditionTag.IsValid() || bEndingPlay ||
		(HasCondition(ConditionTag) && !Definition->bRefreshDurationOnReapply))
	{
		return false;
	}

	return StartCondition(*Definition);
}

bool UFE_CharacterStatusComponent::RemoveCondition(FGameplayTag ConditionTag)
{
	AActor* OwnerActor = GetOwner();
	FActiveConditionRuntime* Runtime = ActiveConditionRuntime.Find(ConditionTag);
	if (!OwnerActor || !OwnerActor->HasAuthority() || !Runtime)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = Runtime->AbilitySystem.Get();
	return ASC && ASC->RemoveActiveGameplayEffect(Runtime->LifetimeEffectHandle);
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

bool UFE_CharacterStatusComponent::StartCondition(const FFE_CharacterConditionDefinition& Definition)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC || ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead)) { return false; }
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		UFE_ConditionLifetimeGameplayEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
	if (!Spec.IsValid()) { return false; }
	Spec.Data->SetDuration(Definition.Duration > 0.0f ? Definition.Duration : UGameplayEffect::INFINITE_DURATION, true);
	Spec.Data->Period = Definition.TickInterval > 0.0f && Definition.HealthLossPerTick > 0.0f ? Definition.TickInterval : 0.0f;
	Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_Condition_HealthLoss,
		Spec.Data->Period > 0.0f ? -Definition.HealthLossPerTick : 0.0f);
	Spec.Data->DynamicGrantedTags.AddTag(Definition.ConditionTag);
	Spec.Data->AddDynamicAssetTag(Definition.ConditionTag);
	const FActiveGameplayEffectHandle LifetimeHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!LifetimeHandle.IsValid()) { return false; } // Immunity must not leave tags, UI or timers behind.

	FActiveGameplayEffectHandle ExtraHandle;
	if (Definition.AppliedEffectClass)
	{
		FGameplayEffectSpecHandle ExtraSpec = ASC->MakeOutgoingSpec(
			Definition.AppliedEffectClass, 1.0f, ASC->MakeEffectContext());
		if (ExtraSpec.IsValid())
		{
			ExtraSpec.Data->AddDynamicAssetTag(Definition.ConditionTag);
			if (ExtraSpec.Data->Def->DurationPolicy != EGameplayEffectDurationType::Instant)
			{
				// Only the lifetime effect expires; extensions/cleansing also govern these modifiers.
				ExtraSpec.Data->SetDuration(UGameplayEffect::INFINITE_DURATION, true);
			}
			ExtraHandle = ASC->ApplyGameplayEffectSpecToSelf(*ExtraSpec.Data.Get());
		}
		if (!ExtraHandle.WasSuccessfullyApplied())
		{
			ASC->RemoveActiveGameplayEffect(LifetimeHandle);
			return false;
		}
	}

	if (ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead))
	{
		ASC->RemoveActiveGameplayEffect(ExtraHandle);
		ASC->RemoveActiveGameplayEffect(LifetimeHandle);
		return false;
	}
	// Replace the previous instance only after the replacement passed all GAS application checks.
	RemoveCondition(Definition.ConditionTag);
	FActiveConditionRuntime& Runtime = ActiveConditionRuntime.Add(Definition.ConditionTag);
	Runtime.LifetimeEffectHandle = LifetimeHandle;
	Runtime.AppliedEffectHandle = ExtraHandle;
	Runtime.AbilitySystem = ASC;
	if (auto* Removed = ASC->OnGameplayEffectRemoved_InfoDelegate(LifetimeHandle))
	{
		Removed->AddUObject(this, &ThisClass::HandleConditionEffectRemoved, Definition.ConditionTag, true);
	}
	if (auto* Removed = ASC->OnGameplayEffectRemoved_InfoDelegate(ExtraHandle))
	{
		Removed->AddUObject(this, &ThisClass::HandleConditionEffectRemoved, Definition.ConditionTag, false);
	}
	if (auto* TimeChanged = ASC->OnGameplayEffectTimeChangeDelegate(LifetimeHandle))
	{
		TimeChanged->AddUObject(this, &ThisClass::HandleConditionTimeChanged, Definition.ConditionTag);
	}
	FFE_ActiveCharacterCondition& Active = ActiveConditions.AddDefaulted_GetRef();
	Active.ConditionTag = Definition.ConditionTag;
	Active.EndServerWorldTime = Definition.Duration > 0.0f ? GetServerWorldTime() + Spec.Data->GetDuration() : 0.0;
	NotifyConditionsChanged();
	return true;
}

void UFE_CharacterStatusComponent::HandleConditionEffectRemoved(
	const FGameplayEffectRemovalInfo& Info, FGameplayTag Tag, bool bLifetimeEffect)
{
	const FActiveConditionRuntime* Runtime = ActiveConditionRuntime.Find(Tag);
	if (!Runtime) { return; }
	if (!bLifetimeEffect)
	{
		// Natural extra-effect expiry is not cleansing; the lifetime GE decides natural expiration.
		if (Info.bPrematureRemoval) { RemoveCondition(Tag); }
		return;
	}
	const FActiveConditionRuntime RemovedRuntime = *Runtime;
	ActiveConditionRuntime.Remove(Tag); // Remove before nested GAS removal delegates can re-enter.
	ActiveConditions.RemoveAll([Tag](const FFE_ActiveCharacterCondition& Active) { return Active.ConditionTag == Tag; });
	if (UAbilitySystemComponent* ASC = RemovedRuntime.AbilitySystem.Get())
	{
		ASC->RemoveActiveGameplayEffect(RemovedRuntime.AppliedEffectHandle);
	}
	NotifyConditionsChanged();
	const FFE_CharacterConditionDefinition* Definition = FindConditionDefinition(Tag);
	if (!bEndingPlay && !Info.bPrematureRemoval && Definition &&
		Definition->ExpirationBehavior == EFE_ConditionExpirationBehavior::KillOwner)
	{
		if (UAbilitySystemComponent* ASC = RemovedRuntime.AbilitySystem.Get())
		{
			ApplyFixedHealthLoss(ASC->GetNumericAttribute(UFallenEraAttributeSet::GetHealthAttribute()));
		}
	}
}

void UFE_CharacterStatusComponent::HandleConditionTimeChanged(
	FActiveGameplayEffectHandle Handle, float StartTime, float Duration, FGameplayTag Tag)
{
	FFE_ActiveCharacterCondition* Active = ActiveConditions.FindByPredicate(
		[Tag](const FFE_ActiveCharacterCondition& Condition) { return Condition.ConditionTag == Tag; });
	if (Active)
	{
		Active->EndServerWorldTime = Duration > 0.0f && GetWorld()
			? GetServerWorldTime() + FMath::Max(0.0f, StartTime + Duration - GetWorld()->GetTimeSeconds()) : 0.0;
		NotifyConditionsChanged();
	}
}

void UFE_CharacterStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	RemoveAllConditions(); // PlayerState's ASC can outlive this pawn.
	ActiveConditionRuntime.Reset();
	Super::EndPlay(EndPlayReason);
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
