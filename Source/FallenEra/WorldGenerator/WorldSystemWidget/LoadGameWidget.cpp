// Fill out your copyright notice in the Description page of Project Settings.


#include "LoadGameWidget.h"

#include "Components/Button.h"

void ULoadGameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	
	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddDynamic(this, &ULoadGameWidget::OnEixtClicked);
	}
	
}

void ULoadGameWidget::OnEixtClicked()
{
	OnLoadGameExitCliked.Broadcast();
}
