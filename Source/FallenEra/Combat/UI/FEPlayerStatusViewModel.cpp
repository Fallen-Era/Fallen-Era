#include "Combat/UI/FEPlayerStatusViewModel.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Component/FECharacterStatusComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "FallenEraPlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

namespace
{
	TArray<FGameplayAttribute> StatusAttributes()
	{
		return { UFallenEraAttributeSet::GetHealthAttribute(), UFallenEraAttributeSet::GetMaxHealthAttribute(),
			UFallenEraAttributeSet::GetStaminaAttribute(), UFallenEraAttributeSet::GetMaxStaminaAttribute() };
	}
}

void UFE_PlayerStatusViewModel::Initialize(APlayerController* PlayerController)
{
	if (Controller.Get() != PlayerController)
	{
		Shutdown();
		Controller = PlayerController;
		if (PlayerController)
		{
			PawnChangedHandle = PlayerController->GetOnNewPawnNotifier().AddUObject(this, &ThisClass::HandlePawnChanged);
			if (AFallenEraPlayerController* FEController = Cast<AFallenEraPlayerController>(PlayerController))
			{
				PlayerStateChangedHandle = FEController->OnPlayerStateReady().AddUObject(this, &ThisClass::RefreshSources);
			}
		}
	}
	RefreshSources();
}

void UFE_PlayerStatusViewModel::Shutdown()
{
	if (APlayerController* PC = Controller.Get())
	{
		PC->GetOnNewPawnNotifier().Remove(PawnChangedHandle);
		if (AFallenEraPlayerController* FEController = Cast<AFallenEraPlayerController>(PC))
		{
			FEController->OnPlayerStateReady().Remove(PlayerStateChangedHandle);
		}
	}
	Controller.Reset();
	UnbindSources();
}

void UFE_PlayerStatusViewModel::UnbindSources()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		const TArray<FGameplayAttribute> Attributes = StatusAttributes();
		for (int32 Index = 0; Index < AttributeChangedHandles.Num(); ++Index)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Attributes[Index]).Remove(AttributeChangedHandles[Index]);
		}
	}
	if (UFE_CharacterStatusComponent* Status = StatusComponent.Get())
	{
		Status->OnActiveConditionsChanged().Remove(ConditionsChangedHandle);
	}
	AttributeChangedHandles.Reset();
	ConditionsChangedHandle.Reset();
	AbilitySystem.Reset();
	StatusComponent.Reset();
}

void UFE_PlayerStatusViewModel::HandlePawnChanged(APawn* Pawn) { RefreshSources(); }

void UFE_PlayerStatusViewModel::RefreshSources()
{
	APlayerController* PC = Controller.Get();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UAbilitySystemComponent* ASC = PC ? UFE_CombatComponent::FindAbilitySystemComponent(PC->PlayerState) : nullptr;
	UFE_CharacterStatusComponent* Status = Pawn ? Pawn->FindComponentByClass<UFE_CharacterStatusComponent>() : nullptr;
	if (AbilitySystem.Get() != ASC || StatusComponent.Get() != Status)
	{
		UnbindSources();
		AbilitySystem = ASC;
		StatusComponent = Status;
		if (ASC)
		{
			for (const FGameplayAttribute& Attribute : StatusAttributes())
			{
				AttributeChangedHandles.Add(ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
					.AddUObject(this, &ThisClass::HandleAttributeChanged));
			}
		}
		if (Status)
		{
			ConditionsChangedHandle = Status->OnActiveConditionsChanged().AddUObject(this, &ThisClass::RefreshConditions);
		}
	}
	RefreshValues();
	RefreshConditions();
}

void UFE_PlayerStatusViewModel::HandleAttributeChanged(const FOnAttributeChangeData& Data) { RefreshValues(); }

void UFE_PlayerStatusViewModel::RefreshValues()
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	const UFallenEraAttributeSet* Attributes = ASC ? ASC->GetSet<UFallenEraAttributeSet>() : nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(Health, Attributes ? Attributes->GetHealth() : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Attributes ? Attributes->GetMaxHealth() : 1.0f);
	UE_MVVM_SET_PROPERTY_VALUE(Stamina, Attributes ? Attributes->GetStamina() : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, Attributes ? Attributes->GetMaxStamina() : 1.0f);
	ValuesChanged.Broadcast();
}

void UFE_PlayerStatusViewModel::RefreshConditions()
{
	Conditions = StatusComponent.IsValid() ? StatusComponent->GetActiveConditionsView() : TArray<FFE_ActiveCharacterCondition>();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Conditions);
	ConditionsChanged.Broadcast();
}

bool UFE_PlayerStatusViewModel::GetConditionDefinition(FGameplayTag Tag, FFE_CharacterConditionDefinition& Definition) const
{
	return StatusComponent.IsValid() && StatusComponent->GetConditionDefinition(Tag, Definition);
}
