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
struct FWorldHeightmapData
{
	GENERATED_BODY()
	
	UPROPERTY(SaveGame)
	FIntPoint Size = FIntPoint::ZeroValue;
	
	// 인접 샘플 간 거리(cm)
	UPROPERTY(SaveGame)
	float SampleStepCm = 100.f;
	
	// (0, 0) 샘플의 월드 XY 위치
	UPROPERTY(SaveGame)
	FVector2D Origin = FVector2D::ZeroVector;
	
	// uint16 -> 실제 월드 높이(cm) 복원용 
	UPROPERTY(SaveGame)
	float MinHeightCm = 0.f;
	
	UPROPERTY(SaveGame)
	float MaxHeightCm = 0.f;
	
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
