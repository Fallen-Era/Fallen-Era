// Fill out your copyright notice in the Description page of Project Settings.


#include "MainMenuWidget.h"

#include "Components/Button.h"

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	Btn_NewGame->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleNewGameClicked);
	Btn_LoadGame->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleLoadGameClicked);
}

void UMainMenuWidget::HandleNewGameClicked()
{
	OnNewWorldRequested.Broadcast();
}

void UMainMenuWidget::HandleLoadGameClicked()
{
	OnLoadWorldRequested.Broadcast();
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
}
