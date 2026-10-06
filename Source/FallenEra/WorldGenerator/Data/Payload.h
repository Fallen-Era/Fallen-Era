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
	FIntPoint WorldSize = FIntPoint::ZeroValue;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 WorldSeed;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWorldDiffculty Diffculty = EWorldDiffculty::Normal;
	
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
	EWorldDiffculty Diffculty = EWorldDiffculty::Normal;
	
	UPROPERTY(VisibleAnywhere)
	FIntPoint WorldSize = FIntPoint(0, 0);
	
	UPROPERTY(VisibleAnywhere)
	FString ProfileSlotName;
};

USTRUCT(BlueprintType)
struct FALLENERA_API FWorldDefinitionData
{
	GENERATED_BODY()
	
	UPROPERTY()
	int32 SchemaVersion = 1;
	
	UPROPERTY()
	FGuid WorldId;
	
	UPROPERTY()
	FString DisplayName;
	
	UPROPERTY()
	int32 WorldSeed = 0;
	
	UPROPERTY()
	FIntPoint WorldSize = FIntPoint::ZeroValue;
	
	UPROPERTY()
	EWorldDiffculty Diffculty = EWorldDiffculty::Normal;
	
};

