// Fill out your copyright notice in the Description page of Project Settings.


#include "MainPanelWidget.h"

#include "MainMenuWidget.h"
#include "NewGameWidget.h"
#include "LoadGameWidget.h"
#include "NetworkMessage.h"

#include "Components/WidgetSwitcher.h"
#include "WorldGenerator/WorldRegistrySubsystem.h"

#include "WorldGenerator/GameplayTag/WorldGameplayTag.h"
#include "WorldGenerator/Save/WorldRegistrySaveGame.h"

void UMainPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	WidgetSwitcher->SetActiveWidget(MainMenu);
	
	MainMenu->OnNewWorldRequested.AddDynamic(this, &UMainPanelWidget::HandleNewGameRequested);
	MainMenu->OnLoadWorldRequested.AddDynamic(this, &UMainPanelWidget::HandleLoadGameRequested);
	
	NewGame->OnNewGameExitClicked.AddDynamic(this, &UMainPanelWidget::HandleMainMenuRequested);
	LoadGame->OnLoadGameExitCliked.AddDynamic(this, &UMainPanelWidget::HandleMainMenuRequested);
	
	
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(this);
	
	MessageBoxListenerHandle =
		MessageSubsystem.RegisterListener<FMessageBoxRequest>(
		WorldGameplayTag::TAG_Event_World_Widget_MessageBox,
		this,
		&ThisClass::MessageBoxPopup);
	
	
}

void UMainPanelWidget::HandleNewGameRequested()
{
	WidgetSwitcher->SetActiveWidget(NewGame);
}

void UMainPanelWidget::HandleLoadGameRequested()
{
	UWorldRegistrySubsystem* WorldRegistrySubsystem = 
	GetGameInstance()->GetSubsystem<UWorldRegistrySubsystem>();
	
	UWorldRegistrySaveGame* Registry = WorldRegistrySubsystem->GetRegistry();
	
	LoadGame->UpdateRegistryElements(Registry->GetWorldRegistryList());
	
	WidgetSwitcher->SetActiveWidget(LoadGame);
}

void UMainPanelWidget::HandleMainMenuRequested()
{
	WidgetSwitcher->SetActiveWidget(MainMenu);
}

void UMainPanelWidget::MessageBoxPopup(FGameplayTag Channel, const FMessageBoxRequest& Request)
{
	
}
