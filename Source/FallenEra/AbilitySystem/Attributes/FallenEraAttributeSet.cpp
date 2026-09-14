#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UFallenEraAttributeSet::UFallenEraAttributeSet()
	: Health(100.0f)
	, MaxHealth(100.0f)
	, Stamina(100.0f)
	, MaxStamina(100.0f)
	, Damage(0.0f)
	, Healing(0.0f)
{
}

void UFallenEraAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
}

void UFallenEraAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent();

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = FMath::Max(GetDamage(), 0.0f);
		SetDamage(0.0f);
		if (LocalDamage > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - LocalDamage, 0.0f, GetMaxHealth()));
			if (AbilitySystemComponent)
			{
				FGameplayCueParameters CueParameters;
				CueParameters.RawMagnitude = LocalDamage;
				AbilitySystemComponent->ExecuteGameplayCue(FallenEraGameplayTags::GameplayCue_Damage, CueParameters);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float LocalHealing = FMath::Max(GetHealing(), 0.0f);
		SetHealing(0.0f);
		if (LocalHealing > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() + LocalHealing, 0.0f, GetMaxHealth()));
			if (AbilitySystemComponent)
			{
				FGameplayCueParameters CueParameters;
				CueParameters.RawMagnitude = LocalHealing;
				AbilitySystemComponent->ExecuteGameplayCue(FallenEraGameplayTags::GameplayCue_Heal, CueParameters);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute() || Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute() || Data.EvaluatedData.Attribute == GetMaxStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}

	UpdateDeadStateTag();
}

void UFallenEraAttributeSet::UpdateDeadStateTag()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent();
	const AActor* OwnerActor = AbilitySystemComponent ? AbilitySystemComponent->GetOwnerActor() : nullptr;
	if (AbilitySystemComponent && OwnerActor && OwnerActor->HasAuthority())
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(
			FallenEraGameplayTags::State_Dead,
			GetHealth() <= 0.0f ? 1 : 0,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void UFallenEraAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
}

void UFallenEraAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFallenEraAttributeSet, Health, OldValue);
}

void UFallenEraAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFallenEraAttributeSet, MaxHealth, OldValue);
}

void UFallenEraAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFallenEraAttributeSet, Stamina, OldValue);
}

void UFallenEraAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFallenEraAttributeSet, MaxStamina, OldValue);
}
