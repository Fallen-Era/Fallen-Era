// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldElement.generated.h"

class UTextBlock;

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldElement : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetWorldName(const FText& NewWorldName);
	void SetWorldInfo(const FText& NewWorldInfo);
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_WorldName;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_WorldInfo;
};
