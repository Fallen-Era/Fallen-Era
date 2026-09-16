// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldGeneratorMenu.h"

#include "Components/Button.h"
#include "Components/EditableText.h"

#include "FallenEra/AbilitySystem/FallenEraGameplayTags.h"
// #include "GameFramework/GameplayMessageSubsystem.h"


void UWorldGeneratorMenu::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (Btn_WorldGen)
	{
		Btn_WorldGen->OnClicked.AddDynamic(this, &UWorldGeneratorMenu::OnWorldGenerateClicked);
	}
	
	
}

void UWorldGeneratorMenu::OnWorldGenerateClicked()
{
	if (!Size_X || !Size_Y)
	{
		return;
	}
	
	const int32 X = FCString::Atoi(*Size_X->GetText().ToString());
	const int32 Y = FCString::Atoi(*Size_Y->GetText().ToString());
	
	if (X <= 0 || Y <= 0)
	{
		return;
	}
	
	FWorldGenerateRequest Request;
	Request.WorldSize = FIntPoint(X,Y);
	
	// UGameplayMeesage
}
