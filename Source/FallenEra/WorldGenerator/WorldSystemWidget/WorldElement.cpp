// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldElement.h"

#include "Components/TextBlock.h"

void UWorldElement::SetWorldName(const FText& NewWorldName)
{
	TXT_WorldName->SetText(NewWorldName);
}

void UWorldElement::SetWorldInfo(const FText& NewWorldInfo)
{
	TXT_WorldInfo->SetText(NewWorldInfo);
}
