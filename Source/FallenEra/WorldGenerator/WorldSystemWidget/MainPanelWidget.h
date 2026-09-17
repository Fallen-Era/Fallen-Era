// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainPanelWidget.generated.h"

class UWidgetSwitcher;
class UMainMenuWidget;
class UNewGameWidget;
class ULoadGameWidget;

/**
 * 
 */
UCLASS()
class FALLENERA_API UMainPanelWidget : public UUserWidget
{
	GENERATED_BODY()
	
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UMainMenuWidget> MainMenu;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UNewGameWidget> NewGame;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<ULoadGameWidget> LoadGame;
	
protected:
	virtual void NativeConstruct() override;
	
private:
	UFUNCTION()
	void HandleNewGameRequested();
	
	UFUNCTION()
	void HandleLoadGameRequested();
};
