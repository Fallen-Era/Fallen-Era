#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "HT/Component/CombatComponent.h"
#include "Net/UnrealNetwork.h"

UFallenEraAttributeSet::UFallenEraAttributeSet()
	: Health(100.0f)
	, MaxHealth(100.0f)
	, Stamina(100.0f)
	, MaxStamina(100.0f)
	, AttackPower(0.0f)
	, DefensePower(0.0f)
	, KnockbackResistance(0.0f)
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
	else if (Attribute == GetAttackPowerAttribute() || Attribute == GetDefensePowerAttribute() || Attribute == GetKnockbackResistanceAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
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

	if (Data.EvaluatedData.Attribute == GetAttackPowerAttribute())
	{
		SetAttackPower(FMath::Max(GetAttackPower(), 0.0f));
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
		if (Data.EvaluatedData.Attribute == GetHealthAttribute() && Data.EvaluatedData.Magnitude < 0.0f && AbilitySystemComponent)
		{
			FGameplayCueParameters CueParameters;
			CueParameters.RawMagnitude = FMath::Abs(Data.EvaluatedData.Magnitude);
			AbilitySystemComponent->ExecuteGameplayCue(FallenEraGameplayTags::GameplayCue_Damage, CueParameters);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetKnockbackResistanceAttribute())
	{
		SetKnockbackResistance(FMath::Max(GetKnockbackResistance(), 0.0f));
	}
	else if (Data.EvaluatedData.Attribute == GetDefensePowerAttribute())
	{
		SetDefensePower(FMath::Max(GetDefensePower(), 0.0f));
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
	AActor* OwnerActor = AbilitySystemComponent ? AbilitySystemComponent->GetOwnerActor() : nullptr;
	if (AbilitySystemComponent && OwnerActor && OwnerActor->HasAuthority())
	{
		const bool bWasDead = AbilitySystemComponent->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
		const bool bIsDead = GetHealth() <= 0.0f;
		AbilitySystemComponent->SetLooseGameplayTagCount(
			FallenEraGameplayTags::State_Dead,
			bIsDead ? 1 : 0,
			EGameplayTagReplicationState::TagAndCountToAll);

		if (!bWasDead && bIsDead)
		{
			AActor* AvatarActor = AbilitySystemComponent->GetAvatarActor();
			if (!AvatarActor)
			{
				AvatarActor = OwnerActor;
			}

			if (UFE_CombatComponent* CombatComponent = AvatarActor
				? AvatarActor->FindComponentByClass<UFE_CombatComponent>()
				: nullptr)
			{
				CombatComponent->HandleDeath();
			}
		}
	}
}

void UFallenEraAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, DefensePower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UFallenEraAttributeSet, KnockbackResistance, COND_None, REPNOTIFY_Always);
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

void UFallenEraAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFallenEraAttributeSet, AttackPower, OldValue);
}

void UFallenEraAttributeSet::OnRep_DefensePower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFallenEraAttributeSet, DefensePower, OldValue);
}

void UFallenEraAttributeSet::OnRep_KnockbackResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UFallenEraAttributeSet, KnockbackResistance, OldValue);
}
