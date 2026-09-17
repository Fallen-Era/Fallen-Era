// Fill out your copyright notice in the Description page of Project Settings.


#include "MainPanelWidget.h"

#include "MainMenuWidget.h"
#include "NewGameWidget.h"
#include "LoadGameWidget.h"
#include "NetworkMessage.h"

#include "Components/WidgetSwitcher.h"

void UMainPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	WidgetSwitcher->SetActiveWidget(MainMenu);
	
	MainMenu->OnNewWorldRequested.AddDynamic(this, &UMainPanelWidget::HandleNewGameRequested);
	MainMenu->OnLoadWorldRequested.AddDynamic(this, &UMainPanelWidget::HandleLoadGameRequested);
}

void UMainPanelWidget::HandleNewGameRequested()
{
	WidgetSwitcher->SetActiveWidget(NewGame);
}

void UMainPanelWidget::HandleLoadGameRequested()
{
	WidgetSwitcher->SetActiveWidget(LoadGame);
}
