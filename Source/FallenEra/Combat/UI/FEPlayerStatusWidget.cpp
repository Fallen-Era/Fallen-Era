#include "Combat/UI/FEPlayerStatusWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "FallenEraPlayerState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameFramework/Pawn.h"
#include "Combat/Component/FECharacterStatusComponent.h"
#include "Combat/UI/FEConditionSlotWidget.h"

UFE_PlayerStatusWidget::UFE_PlayerStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConditionSlotWidgetClass = UFE_ConditionSlotWidget::StaticClass();
}

void UFE_PlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (VerticalBox_ConditionBox)
	{
		// Reverse the panel while counter-rotating each child so new conditions fill bottom-to-top.
		VerticalBox_ConditionBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		VerticalBox_ConditionBox->SetRenderTransformAngle(180.0f);
	}
	RefreshFromPlayerState();
}

void UFE_PlayerStatusWidget::NativeDestruct()
{
	UnbindFromAbilitySystem();
	UnbindFromCharacterStatus();
	Super::NativeDestruct();
}

void UFE_PlayerStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const APawn* OwningPawn = GetOwningPlayerPawn();
	if (!BoundAbilitySystemComponent.IsValid() || !BoundCharacterStatusComponent.IsValid() ||
		(BoundCharacterStatusComponent.IsValid() && BoundCharacterStatusComponent->GetOwner() != OwningPawn))
	{
		RefreshFromPlayerState();
	}
}

void UFE_PlayerStatusWidget::RefreshFromPlayerState()
{
	APawn* OwningPawn = GetOwningPlayerPawn();
	BindToCharacterStatus(OwningPawn
		? OwningPawn->FindComponentByClass<UFE_CharacterStatusComponent>()
		: nullptr);

	AFallenEraPlayerState* PlayerState = GetOwningPlayerState<AFallenEraPlayerState>();
	if (!PlayerState)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = PlayerState->GetAbilitySystemComponent();
	const UFallenEraAttributeSet* AttributeSet = PlayerState->GetAttributeSet();
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	BindToAbilitySystem(AbilitySystemComponent, AttributeSet);
	RefreshAllValues(AttributeSet);
}

void UFE_PlayerStatusWidget::BindToCharacterStatus(UFE_CharacterStatusComponent* StatusComponent)
{
	if (BoundCharacterStatusComponent.Get() == StatusComponent)
	{
		return;
	}

	UnbindFromCharacterStatus();
	BoundCharacterStatusComponent = StatusComponent;
	if (StatusComponent)
	{
		ActiveConditionsChangedHandle = StatusComponent->OnActiveConditionsChanged().AddUObject(
			this, &UFE_PlayerStatusWidget::RebuildConditionSlots);
	}
	RebuildConditionSlots();
}

void UFE_PlayerStatusWidget::UnbindFromCharacterStatus()
{
	if (UFE_CharacterStatusComponent* StatusComponent = BoundCharacterStatusComponent.Get())
	{
		if (ActiveConditionsChangedHandle.IsValid())
		{
			StatusComponent->OnActiveConditionsChanged().Remove(ActiveConditionsChangedHandle);
		}
	}
	ActiveConditionsChangedHandle.Reset();
	BoundCharacterStatusComponent.Reset();
}

void UFE_PlayerStatusWidget::RebuildConditionSlots()
{
	ConditionSlots.Reset();
	if (!VerticalBox_ConditionBox || !ConditionSlotWidgetClass)
	{
		return;
	}

	VerticalBox_ConditionBox->ClearChildren();
	UFE_CharacterStatusComponent* StatusComponent = BoundCharacterStatusComponent.Get();
	if (!StatusComponent)
	{
		return;
	}

	for (const FFE_ActiveCharacterCondition& ActiveCondition : StatusComponent->GetActiveConditions())
	{
		FFE_CharacterConditionDefinition Definition;
		if (!StatusComponent->GetConditionDefinition(ActiveCondition.ConditionTag, Definition))
		{
			continue;
		}

		UFE_ConditionSlotWidget* ConditionSlot = CreateWidget<UFE_ConditionSlotWidget>(
			GetOwningPlayer(), ConditionSlotWidgetClass);
		if (!ConditionSlot)
		{
			continue;
		}
		ConditionSlot->InitializeConditionSlot(
			ActiveCondition.ConditionTag,
			Definition.IconTexture,
			Definition.IconSize,
			Definition.DisplayName,
			ActiveCondition.EndServerWorldTime);
		ConditionSlot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		ConditionSlot->SetRenderTransformAngle(180.0f);
		VerticalBox_ConditionBox->AddChildToVerticalBox(ConditionSlot);
		ConditionSlots.Add(ActiveCondition.ConditionTag, ConditionSlot);
	}
}

void UFE_PlayerStatusWidget::BindToAbilitySystem(
	UAbilitySystemComponent* AbilitySystemComponent,
	const UFallenEraAttributeSet* AttributeSet)
{
	if (BoundAbilitySystemComponent.Get() == AbilitySystemComponent && BoundAttributeSet.Get() == AttributeSet)
	{
		return;
	}

	UnbindFromAbilitySystem();
	BoundAbilitySystemComponent = AbilitySystemComponent;
	BoundAttributeSet = AttributeSet;

	HealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UFallenEraAttributeSet::GetHealthAttribute()).AddUObject(this, &UFE_PlayerStatusWidget::OnHealthChanged);
	MaxHealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UFallenEraAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UFE_PlayerStatusWidget::OnMaxHealthChanged);
	StaminaChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UFallenEraAttributeSet::GetStaminaAttribute()).AddUObject(this, &UFE_PlayerStatusWidget::OnStaminaChanged);
	MaxStaminaChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UFallenEraAttributeSet::GetMaxStaminaAttribute()).AddUObject(this, &UFE_PlayerStatusWidget::OnMaxStaminaChanged);
}

void UFE_PlayerStatusWidget::UnbindFromAbilitySystem()
{
	UAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (AbilitySystemComponent)
	{
		if (HealthChangedHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		}
		if (StaminaChangedHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
		}
		if (MaxStaminaChangedHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetMaxStaminaAttribute()).Remove(MaxStaminaChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();
	BoundAbilitySystemComponent.Reset();
	BoundAttributeSet.Reset();
}

void UFE_PlayerStatusWidget::RefreshAllValues(const UFallenEraAttributeSet* AttributeSet) const
{
	if (!AttributeSet)
	{
		return;
	}

	RefreshHealth(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
	RefreshStamina(AttributeSet->GetStamina(), AttributeSet->GetMaxStamina());
}

void UFE_PlayerStatusWidget::RefreshHealth(float Health, float MaxHealth) const
{
	const float NormalizedHealth = MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f;
	if (ProgressBar_HPBar)
	{
		ProgressBar_HPBar->SetPercent(NormalizedHealth);
	}
	if (Text_HPText)
	{
		Text_HPText->SetText(FText::Format(
			NSLOCTEXT("PlayerStatusWidget", "HealthFormat", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(Health)),
			FText::AsNumber(FMath::RoundToInt(MaxHealth))));
	}
}

void UFE_PlayerStatusWidget::RefreshStamina(float Stamina, float MaxStamina) const
{
	const float NormalizedStamina = MaxStamina > 0.0f ? FMath::Clamp(Stamina / MaxStamina, 0.0f, 1.0f) : 0.0f;
	if (ProgressBar_StaminaBar)
	{
		ProgressBar_StaminaBar->SetPercent(NormalizedStamina);
	}
	if (Text_StaminaText)
	{
		Text_StaminaText->SetText(FText::Format(
			NSLOCTEXT("PlayerStatusWidget", "StaminaFormat", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(Stamina)),
			FText::AsNumber(FMath::RoundToInt(MaxStamina))));
	}
}

void UFE_PlayerStatusWidget::OnHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	const UFallenEraAttributeSet* AttributeSet = BoundAttributeSet.Get();
	if (AttributeSet)
	{
		RefreshHealth(ChangeData.NewValue, AttributeSet->GetMaxHealth());
	}
}

void UFE_PlayerStatusWidget::OnMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	const UFallenEraAttributeSet* AttributeSet = BoundAttributeSet.Get();
	if (AttributeSet)
	{
		RefreshHealth(AttributeSet->GetHealth(), ChangeData.NewValue);
	}
}

void UFE_PlayerStatusWidget::OnStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	const UFallenEraAttributeSet* AttributeSet = BoundAttributeSet.Get();
	if (AttributeSet)
	{
		RefreshStamina(ChangeData.NewValue, AttributeSet->GetMaxStamina());
	}
}

void UFE_PlayerStatusWidget::OnMaxStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	const UFallenEraAttributeSet* AttributeSet = BoundAttributeSet.Get();
	if (AttributeSet)
	{
		RefreshStamina(AttributeSet->GetStamina(), ChangeData.NewValue);
	}
}
