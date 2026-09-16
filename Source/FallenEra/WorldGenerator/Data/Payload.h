// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Payload.generated.h"

USTRUCT(BlueprintType)
struct FALLENERA_API FWorldGenerateRequest
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector2 WorldSize = FIntVector2(0,0);
	
};
