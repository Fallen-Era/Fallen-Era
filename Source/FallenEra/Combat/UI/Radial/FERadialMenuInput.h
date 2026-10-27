#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"

class APlayerController;
class UGameViewportClient;
class IInputProcessor;

/** Balanced owner-local input lease. The wheel never takes keyboard focus away from the viewport. */
struct FALLENERA_API FFE_RadialMenuInput
{
	void Capture(APlayerController* Controller, const FSimpleDelegate& Cancel);
	void Restore();
	bool IsCaptured() const { return bCaptured; }
private:
	TWeakObjectPtr<APlayerController> Owner;
	TWeakObjectPtr<UGameViewportClient> Viewport;
	TSharedPtr<IInputProcessor> Processor;
	EMouseCaptureMode PreviousCapture = EMouseCaptureMode::NoCapture;
	EMouseLockMode PreviousLock = EMouseLockMode::DoNotLock;
	bool bPreviousCursor = false;
	bool bPreviousHideCursor = false;
	bool bPreviousIgnoreInput = false;
	bool bCaptured = false;
};
