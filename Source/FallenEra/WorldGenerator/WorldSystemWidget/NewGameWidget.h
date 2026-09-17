// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Data/Payload.h"
#include "NewGameWidget.generated.h"

class UButton;
class UEditableText;

/**
 * 
 */


UCLASS()
class FALLENERA_API UNewGameWidget : public UUserWidget
{
	GENERATED_BODY()
	
	
	
protected:
	
	virtual void NativeConstruct() override;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_WorldGen;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableText> EDIT_TXT_WorldName;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableText> EDIT_TXT_Size_X;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableText> EDIT_TXT_Size_Y;
	
	FText ValidSizeX;
	FText ValidSizeY;
	
private:
	
	UFUNCTION()
	void OnWorldGenerateClicked();
	
	UFUNCTION()
	void OnTXTSizeXChanged(const FText& Text);
	
	UFUNCTION()
	void OnTXTSizeYChanged(const FText& Text);
	
};
