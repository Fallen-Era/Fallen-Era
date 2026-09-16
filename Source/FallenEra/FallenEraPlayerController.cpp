#include "FallenEraPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "FallenEraCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "FallenEra.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "FallenEraPlayerState.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "HT/UI/PlayerMainWidget.h"

AFallenEraPlayerController::AFallenEraPlayerController()
{
	PlayerCameraManagerClass = AFallenEraCameraManager::StaticClass();
}

void AFallenEraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController() && PlayerMainWidgetClass)
	{
		PlayerMainWidget = CreateWidget<UFE_PlayerMainWidget>(this, PlayerMainWidgetClass);
		if (PlayerMainWidget)
		{
			PlayerMainWidget->AddToViewport(0);
		}
	}

	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogFallenEra, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AFallenEraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

void AFallenEraPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (AFallenEraPlayerState* FallenEraPlayerState = GetPlayerState<AFallenEraPlayerState>())
	{
		if (UFallenEraAbilitySystemComponent* AbilitySystem = FallenEraPlayerState->GetFallenEraAbilitySystemComponent())
		{
			AbilitySystem->ProcessAbilityInput(DeltaTime, bGamePaused);
		}
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}

bool AFallenEraPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
