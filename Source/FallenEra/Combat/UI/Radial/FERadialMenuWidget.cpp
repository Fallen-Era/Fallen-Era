#include "Combat/UI/Radial/FERadialMenuWidget.h"
#include "Combat/UI/Radial/FERadialMenuOptionWidget.h"
#include "Combat/UI/Radial/FERadialMenuViewModel.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void UFE_RadialMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bClosingAfterConfirm = false;
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Visible);
	if (Model && !ValuesHandle.IsValid()) { ValuesHandle = Model->ValuesChanged.AddUObject(this, &ThisClass::RefreshPresentation); }
	RefreshPresentation();
}

bool UFE_RadialMenuWidget::SetMenuModel(UFE_RadialMenuViewModel* InModel)
{
	if (Model) { Model->ValuesChanged.Remove(ValuesHandle); }
	ValuesHandle.Reset();
	Model = InModel;
	if (!SizeBox_Wheel || !CanvasPanel_Options || !Text_Title || !Text_Hint || !OptionWidgetClass.Get()) { return false; }
	if (Model) { ValuesHandle = Model->ValuesChanged.AddUObject(this, &ThisClass::RefreshPresentation); }
	RefreshPresentation();
	return true;
}

void UFE_RadialMenuWidget::NativeDestruct()
{
	if (Model) { Model->ValuesChanged.Remove(ValuesHandle); }
	ValuesHandle.Reset();
	InputLease.Restore();
	if (!bClosingAfterConfirm) { RequestCancel(); }
	Super::NativeDestruct();
}

bool UFE_RadialMenuWidget::CaptureMenuInput()
{
	APlayerController* Controller = GetOwningPlayer();
	if (!Controller || !Controller->IsLocalController()) { return false; }
	InputLease.Capture(Controller, FSimpleDelegate::CreateUObject(this, &ThisClass::CancelMenu));
	return InputLease.IsCaptured();
}

bool UFE_RadialMenuWidget::ConfirmMenu()
{
	if (!Model || bClosingAfterConfirm || bRequestingCancel) { return false; }
	bClosingAfterConfirm = true; // An action can synchronously remove the widget.
	if (!Model->ConfirmSelection()) { bClosingAfterConfirm = false; return false; }
	InputLease.Restore();
	RemoveFromParent();
	return true;
}

void UFE_RadialMenuWidget::RequestCancel()
{
	if (bRequestingCancel) { return; }
	TGuardValue<bool> Guard(bRequestingCancel, true);
	InputLease.Restore();
	CancelRequested.ExecuteIfBound();
	OnCancelRequested.Broadcast();
}

void UFE_RadialMenuWidget::CancelMenu()
{
	RequestCancel();
	// Destruct must not send another cancellation for the same close.
	bClosingAfterConfirm = true;
	RemoveFromParent();
}

void UFE_RadialMenuWidget::UpdateSelectedText(const FText& Text)
{
	if (Text_SelectedItem) { Text_SelectedItem->SetText(Text); }
}

void UFE_RadialMenuWidget::RefreshPresentation()
{
	if (!CanvasPanel_Options || !SizeBox_Wheel || !Text_Title || !Text_Hint) { return; }
	if (!Model)
	{
		CanvasPanel_Options->ClearChildren(); Entries.Reset();
		Text_Title->SetText(FText::GetEmpty()); Text_Hint->SetText(FText::GetEmpty()); UpdateSelectedText(FText::GetEmpty());
		return;
	}
	SizeBox_Wheel->SetWidthOverride(WheelSize);
	SizeBox_Wheel->SetHeightOverride(WheelSize);
	if (Entries.Num() != Model->Items.Num())
	{
		CanvasPanel_Options->ClearChildren(); Entries.Reset();
		for (int32 Index = 0; Index < Model->Items.Num(); ++Index)
		{
			UFE_RadialMenuOptionWidget* Entry = GetOwningPlayer()
				? CreateWidget<UFE_RadialMenuOptionWidget>(GetOwningPlayer(), OptionWidgetClass.Get())
				: CreateWidget<UFE_RadialMenuOptionWidget>(GetWorld(), OptionWidgetClass.Get());
			if (!Entry) { CanvasPanel_Options->ClearChildren(); Entries.Reset(); return; }
			UCanvasPanelSlot* EntrySlot = CanvasPanel_Options->AddChildToCanvas(Entry);
			EntrySlot->SetPosition(FVector2D::ZeroVector);
			EntrySlot->SetSize(FVector2D(WheelSize));
			Entry->SetVisibility(ESlateVisibility::HitTestInvisible);
			Entries.Add(Entry);
		}
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index]->UpdateItem(Model->Items[Index], Index, Model->Items.Num(), Index == Model->SelectedIndex, WheelSize, InnerRadius, OuterRadius);
	}
	Text_Title->SetText(Model->Title);
	Text_Hint->SetText(Model->SelectedIndex == INDEX_NONE ? Model->CancelHint : Model->ConfirmHint);
	UpdateSelectedText(Model->Items.IsValidIndex(Model->SelectedIndex) ? Model->Items[Model->SelectedIndex].Label :
		Model->Items.IsEmpty() ? Model->EmptyText : FText::GetEmpty());
}

FReply UFE_RadialMenuWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Model && SizeBox_Wheel)
	{
		const FGeometry& WheelGeometry = SizeBox_Wheel->GetCachedGeometry();
		const FVector2D Offset = WheelGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - WheelGeometry.GetLocalSize() * 0.5;
		Model->SelectIndex(UFE_RadialMenuViewModel::FindSector(Offset, Model->Items.Num(), InnerRadius, OuterRadius));
	}
	return FReply::Handled();
}

FReply UFE_RadialMenuWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) { return FReply::Handled(); }
FReply UFE_RadialMenuWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) { return FReply::Handled(); }
void UFE_RadialMenuWidget::NativeOnMouseLeave(const FPointerEvent& Event)
{
	if (Model) { Model->SelectIndex(INDEX_NONE); }
	Super::NativeOnMouseLeave(Event);
}
