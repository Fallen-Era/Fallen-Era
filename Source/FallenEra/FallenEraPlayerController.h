// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FallenEraPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UFE_PlayerMainWidget;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class FALLENERA_API AFallenEraPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	AFallenEraPlayerController();

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Root player HUD containing W_PlayerStatus and W_PlayerCrossHair. */
	UPROPERTY(EditAnywhere, Category="UI|Player")
	TSubclassOf<UFE_PlayerMainWidget> PlayerMainWidgetClass;

	UPROPERTY()
	TObjectPtr<UFE_PlayerMainWidget> PlayerMainWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Lets the tag-driven ASC consume Enhanced Input state once per player-input frame. */
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
