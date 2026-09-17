// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VoxelStampRef.h"
#include "WorldGenerator/Data/Payload.h"
#include "WorldProfileSaveGame.generated.h"

class UVoxelLayerStack;

USTRUCT()
struct FWorldStampSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform WorldTransform = FTransform::Identity;

	UPROPERTY()
	FVoxelStampRef Stamp;
};

UCLASS()
class FALLENERA_API UWorldProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FWorldProfileData Profile;

	UPROPERTY()
	TObjectPtr<UVoxelLayerStack> LayerStack;

	UPROPERTY()
	TArray<FWorldStampSnapshot> StampSnapshots;
};