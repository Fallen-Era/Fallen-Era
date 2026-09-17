// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Payload.generated.h"


USTRUCT(BlueprintType)
struct FALLENERA_API FWorldCreateRequest
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint WorldSize;
	
};

// World List에 표시되는 데이터.
USTRUCT(BlueprintType)
struct FALLENERA_API FWorldRegistryEntry
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	FGuid WorldIds;
	
	UPROPERTY(VisibleAnywhere)
	FString DisplayName;
	
	UPROPERTY(VisibleAnywhere)
	FString ProfileSlotName;
};

USTRUCT(Blueprintable)
struct FTerrainData
{
	GENERATED_BODY()
	
	UPROPERTY(SaveGame)
	float VoxelSize = 100.f;
	
	UPROPERTY(SaveGame)
	FIntPoint WorldSize = FIntPoint::ZeroValue;
	
	UPROPERTY(SaveGame)
	FVector2D Origin = FVector2D::ZeroVector;
	
	UPROPERTY(SaveGame)
	float MinHeightCm = 0.f;
	
	UPROPERTY(SaveGame)
	float MaxHeightCm = 0.f;
	
	UPROPERTY(SaveGame)
	FFloatRange HeightRange = FFloatRange(-5000.f, 5000.f);
	
	// 16-bit grayscale PNG
	UPROPERTY(SaveGame)
	TArray<uint8> CompressedHeightPng;
	
	// Nan? / void 영역을 쓴다면 별도 저장
	UPROPERTY(SaveGame)
	TArray<uint8> ValidityMask;
};

USTRUCT(BlueprintType)
struct FALLENERA_API FWorldProfileData
{
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid WorldIds;
	
	UPROPERTY()
	int32 SchemaVersion = 1;
	
	// Seed
	
	UPROPERTY()
	FIntPoint WorldSize;
	
	// GeneratorVersion
	
	// GenerationOptions
	
};



USTRUCT(BlueprintType)
struct FALLENERA_API FWorldGenerateRequest
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector2 WorldSize = FIntVector2(0,0);
	
};
