#include "Combat/UI/FEPlayerStatusWidget.h"
#include "Combat/UI/FEPlayerStatusViewModel.h"
#include "Combat/UI/FEConditionSlotWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

namespace { constexpr float BottomUpPanelRotation = 180.0f; }

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
		VerticalBox_ConditionBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		VerticalBox_ConditionBox->SetRenderTransformAngle(BottomUpPanelRotation);
	}
	RefreshFromPlayerState();
}

void UFE_PlayerStatusWidget::NativeDestruct()
{
	if (StatusViewModel)
	{
		StatusViewModel->ValuesChanged.Remove(ValuesChangedHandle);
		StatusViewModel->ConditionsChanged.Remove(ConditionsChangedHandle);
		StatusViewModel->Shutdown();
	}
	ValuesChangedHandle.Reset();
	ConditionsChangedHandle.Reset();
	Super::NativeDestruct();
}

void UFE_PlayerStatusWidget::RefreshFromPlayerState()
{
	if (!StatusViewModel) { StatusViewModel = NewObject<UFE_PlayerStatusViewModel>(this); }
	if (!ValuesChangedHandle.IsValid())
	{
		ValuesChangedHandle = StatusViewModel->ValuesChanged.AddUObject(this, &ThisClass::RefreshValues);
		ConditionsChangedHandle = StatusViewModel->ConditionsChanged.AddUObject(this, &ThisClass::RebuildConditionSlots);
	}
	StatusViewModel->Initialize(GetOwningPlayer());
}

void UFE_PlayerStatusWidget::RefreshValues()
{
	RefreshHealth(StatusViewModel->Health, StatusViewModel->MaxHealth);
	RefreshStamina(StatusViewModel->Stamina, StatusViewModel->MaxStamina);
}

void UFE_PlayerStatusWidget::RebuildConditionSlots()
{
	if (!VerticalBox_ConditionBox || !ConditionSlotWidgetClass || !StatusViewModel) { return; }
	TSet<FGameplayTag> ActiveTags;
	for (const auto& Active : StatusViewModel->Conditions) { ActiveTags.Add(Active.ConditionTag); }
	for (auto It = ConditionSlots.CreateIterator(); It; ++It)
	{
		if (!ActiveTags.Contains(It.Key()))
		{
			if (It.Value()) { It.Value()->RemoveFromParent(); }
			ConditionEndTimes.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
	for (const auto& Active : StatusViewModel->Conditions)
	{
		const FGameplayTag Tag = Active.ConditionTag;
		UFE_ConditionSlotWidget* ConditionSlot = ConditionSlots.FindRef(Tag);
		const double* EndTime = ConditionEndTimes.Find(Tag);
		if (ConditionSlot && EndTime && *EndTime == Active.EndServerWorldTime) { continue; }
		FFE_CharacterConditionDefinition Definition;
		if (!StatusViewModel->GetConditionDefinition(Tag, Definition)) { continue; }
		if (!ConditionSlot)
		{
			ConditionSlot = CreateWidget<UFE_ConditionSlotWidget>(GetOwningPlayer(), ConditionSlotWidgetClass);
			if (!ConditionSlot) { continue; }
			ConditionSlot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			ConditionSlot->SetRenderTransformAngle(BottomUpPanelRotation);
			VerticalBox_ConditionBox->AddChildToVerticalBox(ConditionSlot);
			ConditionSlots.Add(Tag, ConditionSlot);
		}
		ConditionSlot->InitializeConditionSlot(Tag, Definition.IconTexture, Definition.IconSize,
			Definition.DisplayName, Active.EndServerWorldTime);
		ConditionEndTimes.Add(Tag, Active.EndServerWorldTime);
	}
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
