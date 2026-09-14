// Copyright Epic Games, Inc. All Rights Reserved.

#include "FallenEraGameMode.h"
#include "FallenEraPlayerState.h"

AFallenEraGameMode::AFallenEraGameMode()
{
	PlayerStateClass = AFallenEraPlayerState::StaticClass();
}
