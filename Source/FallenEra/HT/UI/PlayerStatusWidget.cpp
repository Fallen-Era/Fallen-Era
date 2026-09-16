#include "HT/UI/PlayerStatusWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "FallenEraPlayerState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UFE_PlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromPlayerState();
}

void UFE_PlayerStatusWidget::NativeDestruct()
{
	UnbindFromAbilitySystem();
	Super::NativeDestruct();
}

void UFE_PlayerStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BoundAbilitySystemComponent.IsValid())
	{
		RefreshFromPlayerState();
	}
}

void UFE_PlayerStatusWidget::RefreshFromPlayerState()
{
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
