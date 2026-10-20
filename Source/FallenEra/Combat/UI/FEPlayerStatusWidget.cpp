#include "Combat/UI/FEPlayerStatusWidget.h"
#include "Combat/UI/FEPlayerStatusViewModel.h"
#include "Combat/UI/FEConditionSlotWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace { constexpr float BottomUpPanelRotation = 180.0f; }

UFE_PlayerStatusWidget::UFE_PlayerStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConditionSlotWidgetClass = UFE_ConditionSlotWidget::StaticClass();
}

void UFE_PlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ProgressBar_StaminaBar && !bStaminaBarColorCached)
	{
		OriginalStaminaBarColor = ProgressBar_StaminaBar->GetFillColorAndOpacity();
		bStaminaBarColorCached = true;
	}
	if (VerticalBox_ConditionBox)
	{
		VerticalBox_ConditionBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		VerticalBox_ConditionBox->SetRenderTransformAngle(BottomUpPanelRotation);
	}
	RefreshFromPlayerState();
}

void UFE_PlayerStatusWidget::NativeDestruct()
{
	StopStaminaBlink();
	if (StatusViewModel)
	{
		StatusViewModel->ValuesChanged.Remove(ValuesChangedHandle);
		StatusViewModel->ConditionsChanged.Remove(ConditionsChangedHandle);
		StatusViewModel->AmmoChanged.Remove(AmmoChangedHandle);
		StatusViewModel->StaminaActionRejected.Remove(StaminaRejectedHandle);
		StatusViewModel->Shutdown();
	}
	ValuesChangedHandle.Reset();
	ConditionsChangedHandle.Reset();
	AmmoChangedHandle.Reset();
	StaminaRejectedHandle.Reset();
	Super::NativeDestruct();
}

void UFE_PlayerStatusWidget::RefreshFromPlayerState()
{
	if (!StatusViewModel) { StatusViewModel = NewObject<UFE_PlayerStatusViewModel>(this); }
	if (!ValuesChangedHandle.IsValid())
	{
		ValuesChangedHandle = StatusViewModel->ValuesChanged.AddUObject(this, &ThisClass::RefreshValues);
		ConditionsChangedHandle = StatusViewModel->ConditionsChanged.AddUObject(this, &ThisClass::RebuildConditionSlots);
		AmmoChangedHandle = StatusViewModel->AmmoChanged.AddUObject(this, &ThisClass::RefreshAmmo);
		StaminaRejectedHandle = StatusViewModel->StaminaActionRejected.AddUObject(this, &ThisClass::BlinkStaminaBar);
	}
	StatusViewModel->Initialize(GetOwningPlayer());
}

void UFE_PlayerStatusWidget::BlinkStaminaBar()
{
	UWorld* World = GetWorld();
	if (!ProgressBar_StaminaBar || !World || bStaminaBlinking) { return; }
	bStaminaBlinking = true;
	OriginalStaminaBarOpacity = ProgressBar_StaminaBar->GetRenderOpacity();
	StaminaBlinkStep = 0;
	UpdateStaminaBlink();
	World->GetTimerManager().SetTimer(StaminaBlinkTimer, this, &ThisClass::UpdateStaminaBlink,
		FMath::Max(0.02f, StaminaBlinkInterval), true);
}

void UFE_PlayerStatusWidget::UpdateStaminaBlink()
{
	if (!ProgressBar_StaminaBar || StaminaBlinkStep >= FMath::Clamp(StaminaBlinkCount, 1, 10) * 2)
	{
		StopStaminaBlink();
		return;
	}
	const bool bDim = StaminaBlinkStep++ % 2 == 0;
	// Opacity-only feedback preserves the exhausted gray fill and the cached authored color.
	ProgressBar_StaminaBar->SetRenderOpacity(OriginalStaminaBarOpacity *
		(bDim ? FMath::Clamp(StaminaBlinkDimOpacity, 0.0f, 1.0f) : 1.0f));
}

void UFE_PlayerStatusWidget::StopStaminaBlink()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaBlinkTimer);
	}
	if (bStaminaBlinking && ProgressBar_StaminaBar) { ProgressBar_StaminaBar->SetRenderOpacity(OriginalStaminaBarOpacity); }
	bStaminaBlinking = false;
}

void UFE_PlayerStatusWidget::RefreshValues()
{
	RefreshHealth(StatusViewModel->Health, StatusViewModel->MaxHealth);
	RefreshStamina(StatusViewModel->Stamina, StatusViewModel->MaxStamina);
}

void UFE_PlayerStatusWidget::RefreshAmmo()
{
	if (!StatusViewModel) { return; }
	const ESlateVisibility AmmoVisibility = StatusViewModel->bUsesAmmo ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	HorizontalBox_AmmoStatusBox->SetVisibility(AmmoVisibility);
	if (Text_LoadedAmmo)
	{
		Text_LoadedAmmo->SetText(FText::AsNumber(StatusViewModel->LoadedAmmo));
	}
	if (Text_ReserveAmmo)
	{
		Text_ReserveAmmo->SetText(FText::AsNumber(StatusViewModel->ReserveAmmo));
	}
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
		ProgressBar_StaminaBar->SetFillColorAndOpacity(StatusViewModel && StatusViewModel->bStaminaExhausted
			? FLinearColor(0.4f, 0.4f, 0.4f, OriginalStaminaBarColor.A) : OriginalStaminaBarColor);
	}
	if (Text_StaminaText)
	{
		Text_StaminaText->SetText(FText::Format(
			NSLOCTEXT("PlayerStatusWidget", "StaminaFormat", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(Stamina)),
			FText::AsNumber(FMath::RoundToInt(MaxStamina))));
	}
}
