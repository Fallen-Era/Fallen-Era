// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldRegistryElement.h"


#include "Components/Button.h"
#include "Components/TextBlock.h"

void UWorldRegistryElement::NativeConstruct()
{
	Super::NativeConstruct();
	
	NormalStyle = Btn_Interaction->GetStyle();
	
	FocusedStyle = NormalStyle;
	
	FocusedStyle.Normal.TintColor = FSlateColor(FColor::Yellow);
	FocusedStyle.Hovered.TintColor = FSlateColor(FColor::Yellow);
	FocusedStyle.Pressed.TintColor = FSlateColor(FColor::Yellow);
	if (Btn_Interaction)
	{
		Btn_Interaction->OnClicked.AddDynamic(this, &UWorldRegistryElement::OnClickedInteraction);
	}
}

void UWorldRegistryElement::OnClickedInteraction()
{
	OnFocusInteraction.Broadcast(this);
}

void UWorldRegistryElement::Focused()
{
	Btn_Interaction->SetStyle(FocusedStyle);
}

void UWorldRegistryElement::UnFocused()
{
	Btn_Interaction->SetStyle(NormalStyle);
}

void UWorldRegistryElement::SetWorldName(const FText& NewWorldName)
{
	TXT_WorldName->SetText(NewWorldName);
}

void UWorldRegistryElement::SetWorldInfo(const FText& NewWorldInfo)
{
	TXT_WorldInfo->SetText(NewWorldInfo);
}


