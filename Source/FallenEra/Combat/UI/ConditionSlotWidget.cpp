#include "Combat/UI/ConditionSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"

UFE_ConditionSlotWidget::UFE_ConditionSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MinuteTextFormat = NSLOCTEXT("ConditionSlotWidget", "DefaultMinuteFormat", "{0}m");
	SecondTextFormat = NSLOCTEXT("ConditionSlotWidget", "DefaultSecondFormat", "{0}s");
	TimePartSeparator = NSLOCTEXT("ConditionSlotWidget", "DefaultTimePartSeparator", " ");
}

void UFE_ConditionSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CreateFallbackLayout();
	RefreshVisuals();
}

void UFE_ConditionSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshRemainingTime();
}

void UFE_ConditionSlotWidget::InitializeConditionSlot(
	FGameplayTag InConditionTag,
	UTexture2D* InIconTexture,
	FVector2D InIconSize,
	FText InDisplayName,
	double InEndServerWorldTime)
{
	ConditionTag = InConditionTag;
	IconTexture = InIconTexture;
	IconSize = InIconSize;
	DisplayName = MoveTemp(InDisplayName);
	EndServerWorldTime = InEndServerWorldTime;
	LastDisplayedSeconds = TNumericLimits<int32>::Lowest();
	RefreshVisuals();
}

void UFE_ConditionSlotWidget::CreateFallbackLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ConditionSlotRoot"));
	WidgetTree->RootWidget = RootOverlay;

	Image_ConditionIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_ConditionIcon"));
	RootOverlay->AddChildToOverlay(Image_ConditionIcon);

	Text_RemainingTime = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RemainingTime"));
	Text_RemainingTime->SetJustification(ETextJustify::Center);
	Text_RemainingTime->SetShadowOffset(FVector2D(1.0, 1.0));
	if (UOverlaySlot* TimeSlot = RootOverlay->AddChildToOverlay(Text_RemainingTime))
	{
		TimeSlot->SetHorizontalAlignment(HAlign_Center);
		TimeSlot->SetVerticalAlignment(VAlign_Bottom);
		TimeSlot->SetPadding(FMargin(2.0f));
	}
}

void UFE_ConditionSlotWidget::RefreshVisuals()
{
	if (Image_ConditionIcon)
	{
		Image_ConditionIcon->SetBrushFromTexture(IconTexture, true);
		Image_ConditionIcon->SetDesiredSizeOverride(IconSize);
		Image_ConditionIcon->SetToolTipText(DisplayName);
	}
	RefreshRemainingTime();
}

void UFE_ConditionSlotWidget::RefreshRemainingTime()
{
	if (!Text_RemainingTime)
	{
		return;
	}

	if (EndServerWorldTime <= 0.0)
	{
		if (LastDisplayedSeconds != INDEX_NONE)
		{
			Text_RemainingTime->SetText(NSLOCTEXT("ConditionSlotWidget", "InfiniteTime", "∞"));
			LastDisplayedSeconds = INDEX_NONE;
		}
		return;
	}

	const int32 RemainingSeconds = FMath::Max(
		0, FMath::CeilToInt(EndServerWorldTime - GetServerWorldTime()));
	if (RemainingSeconds == LastDisplayedSeconds)
	{
		return;
	}

	Text_RemainingTime->SetText(FormatRemainingTime(RemainingSeconds));
	LastDisplayedSeconds = RemainingSeconds;
}

FText UFE_ConditionSlotWidget::FormatRemainingTime(int32 RemainingSeconds) const
{
	const int32 Minutes = RemainingSeconds / 60;
	const int32 Seconds = RemainingSeconds % 60;
	const FText MinutePart = FText::Format(MinuteTextFormat, FText::AsNumber(Minutes));
	const FText SecondPart = FText::Format(SecondTextFormat, FText::AsNumber(Seconds));
	return Minutes > 0
		? FText::Format(
			NSLOCTEXT("ConditionSlotWidget", "CombinedTimeParts", "{0}{1}{2}"),
			MinutePart,
			TimePartSeparator,
			SecondPart)
		: SecondPart;
}

double UFE_ConditionSlotWidget::GetServerWorldTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
