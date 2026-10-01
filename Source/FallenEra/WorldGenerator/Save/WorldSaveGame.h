// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VoxelStampRef.h"
#include "WorldGenerator/Data/Payload.h"
#include "WorldSaveGame.generated.h"

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


// 이거 용도 변경해야할듯 월드 생성 및 저장 정보를 스캔하고 레지스트리로 캐시화 시켜둬야 목록에서 빠르게 띄우고 접근 가능할듯.
UCLASS()
class FALLENERA_API UWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FWorldDefinitionData Definition;
	
	UPROPERTY()
	TObjectPtr<UVoxelLayerStack> LayerStack;

	UPROPERTY()
	TArray<FWorldStampSnapshot> StampSnapshots;
};