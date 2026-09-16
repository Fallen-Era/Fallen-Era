// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Data/Payload.h"
#include "WorldGeneratorMenu.generated.h"

class UButton;
class UEditableText;

/**
 * 
 */


UCLASS()
class FALLENERA_API UWorldGeneratorMenu : public UUserWidget
{
	GENERATED_BODY()
	
	
	
protected:
	
	virtual void NativeConstruct() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_WorldGen;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableText> EDIT_TXT_WorldName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableText> EDIT_TXT_Size_X;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableText> EDIT_TXT_Size_Y;
	
private:
	
	UFUNCTION()
	void OnWorldGenerateClicked();
};
