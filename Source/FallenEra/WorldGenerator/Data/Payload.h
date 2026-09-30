// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Payload.generated.h"

UENUM(Blueprintable)
enum class EWorldDiffculty : uint8
{
	Easy UMETA(DisplayName="Easy"),
	Normal UMETA(DisplayName="Normal"),
	Hard UMETA(DisplayName="Hard"),
	VeryHard UMETA(DisplayName="VeryHard"),
	HellGate UMETA(DisplayName="HellGate"),
	InTheHell UMETA(DisplayName="InTheHell"),
};


UENUM(BlueprintType)
enum class EMessageBoxType : uint8
{
	Yes UMETA(DisplayName="Yes"),
	YesNo UMETA(DisplayName="Yes / No"),
};

USTRUCT(BlueprintType)
struct FALLENERA_API FWorldCreateRequest
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint WorldSize;
	
};


USTRUCT(BLueprintType)
struct FALLENERA_API FMessageBoxRequest
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Title;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Message;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMessageBoxType Type = EMessageBoxType::Yes;
	
	
};

// World List에 표시되는 데이터.
USTRUCT(BlueprintType)
struct FALLENERA_API FWorldRegistryData
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	FGuid WorldId;
	
	UPROPERTY(VisibleAnywhere)
	FString DisplayName;
	
	UPROPERTY(VisibleAnywhere)
	EWorldDiffculty Diffculty;
	
	UPROPERTY(VisibleAnywhere)
	FIntVector2 WorldSize;
	
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
	FGuid WorldId;
	
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
