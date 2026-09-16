// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "WorldGenerator/Data/Payload.h"
#include "WorldRegistrySaveGame.generated.h"

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldRegistrySaveGame : public USaveGame
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FWorldRegistryEntry> Worlds;
};
