// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "WorldGenerator/Data/Payload.h"
#include "WorldProfileSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldProfileSaveGame : public USaveGame
{
	GENERATED_BODY()
	
	UPROPERTY()
	FWorldProfileData Profile;
};
