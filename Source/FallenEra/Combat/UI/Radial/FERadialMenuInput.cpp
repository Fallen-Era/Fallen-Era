#include "Combat/UI/Radial/FERadialMenuInput.h"

#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Widgets/SViewport.h"

namespace
{
	class FFE_RadialMenuProcessor final : public IInputProcessor
	{
	public:
		FFE_RadialMenuProcessor(FSimpleDelegate InCancel, TSharedPtr<SViewport> InViewport)
			: Cancel(MoveTemp(InCancel)), Viewport(InViewport) {}
		virtual void Tick(float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
		{
			if (OwnsFocus(SlateApp, Event.GetUserIndex()) && Event.GetKey() == EKeys::Escape) { Cancel.ExecuteIfBound(); return true; }
			return false; // The opening key remains owned by Enhanced Input, including its real release.
		}
		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{
			const TSharedPtr<SViewport> Widget = Viewport.Pin();
			return Widget && OwnsFocus(SlateApp, Event.GetUserIndex()) && Widget->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition());
		}
		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
		{
			return HandleMouseButtonDownEvent(SlateApp, Event);
		}
		// Do not swallow mouse-up: pre-existing held attack/aim inputs must still release.
	private:
		bool OwnsFocus(const FSlateApplication& SlateApp, int32 UserIndex) const
		{
			const TSharedPtr<SViewport> Widget = Viewport.Pin();
			return Widget && SlateApp.GetUserFocusedWidget(UserIndex) == Widget;
		}
		FSimpleDelegate Cancel;
		TWeakPtr<SViewport> Viewport;
	};
}

void FFE_RadialMenuInput::Capture(APlayerController* Controller, const FSimpleDelegate& Cancel)
{
	if (bCaptured || !Controller || !FSlateApplication::IsInitialized()) { return; }
	Owner = Controller;
	Viewport = Controller->GetWorld()->GetGameViewport();
	bPreviousCursor = Controller->bShowMouseCursor;
	bCaptured = true;
	if (UGameViewportClient* Client = Viewport.Get())
	{
		PreviousCapture = Client->GetMouseCaptureMode();
		PreviousLock = Client->GetMouseLockMode();
		bPreviousHideCursor = Client->HideCursorDuringCapture();
		bPreviousIgnoreInput = Client->IgnoreInput();
		FInputModeGameAndUI Mode;
		// Focusing the wheel flushes the held key and generates a new Started on the next held frame.
		// Passive widgets + viewport focus avoid both synthetic releases and reopen loops.
		Mode.SetWidgetToFocus(Client->GetGameViewportWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(Mode);
	}
	Controller->SetIgnoreLookInput(true);
	Controller->bShowMouseCursor = true;
	Processor = MakeShared<FFE_RadialMenuProcessor>(Cancel, Viewport.IsValid() ? Viewport->GetGameViewportWidget() : nullptr);
	FSlateApplication::Get().RegisterInputPreProcessor(Processor, 0);
}

void FFE_RadialMenuInput::Restore()
{
	if (!bCaptured) { return; }
	bCaptured = false; // Restoration can itself dispatch Slate events. Release ownership first.
	if (FSlateApplication::IsInitialized() && Processor) { FSlateApplication::Get().UnregisterInputPreProcessor(Processor); }
	Processor.Reset();
	if (APlayerController* Controller = Owner.Get())
	{
		Controller->bShowMouseCursor = bPreviousCursor;
		if (FSlateApplication::IsInitialized()) { Controller->SetInputMode(FInputModeGameOnly()); }
		Controller->SetIgnoreLookInput(false); // Balance our own increment; never reset another system's lock.
	}
	if (UGameViewportClient* Client = Viewport.Get())
	{
		Client->SetMouseCaptureMode(PreviousCapture);
		Client->SetMouseLockMode(PreviousLock);
		Client->SetHideCursorDuringCapture(bPreviousHideCursor);
		Client->SetIgnoreInput(bPreviousIgnoreInput);
	}
	Owner.Reset();
	Viewport.Reset();
}
