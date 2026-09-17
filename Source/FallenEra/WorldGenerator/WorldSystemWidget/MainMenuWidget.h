// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"


class UButton;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNewGameRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadGameRequested);

/**
 * 
 */
UCLASS()
class FALLENERA_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_NewGame;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_LoadGame;
	
public:
	UPROPERTY(BlueprintAssignable)
	FOnNewGameRequested OnNewWorldRequested;
	
	UPROPERTY(BlueprintAssignable)
	FOnLoadGameRequested OnLoadWorldRequested;
	
protected:
	virtual void NativeOnInitialized() override;
	
	virtual void NativeConstruct() override;
	
private:
	UFUNCTION()
	void HandleNewGameClicked();
	
	UFUNCTION()
	void HandleLoadGameClicked();
	
};
