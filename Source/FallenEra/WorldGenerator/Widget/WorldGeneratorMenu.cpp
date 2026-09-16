// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldGeneratorMenu.h"

#include "Components/Button.h"
#include "Components/EditableText.h"

#include "FallenEra/AbilitySystem/FallenEraGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"


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
	if (!EDIT_TXT_WorldName || 
		!EDIT_TXT_Size_X || 
		!EDIT_TXT_Size_Y)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : Elements are invalid."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}
	
	FWorldCreateRequest Request;
	
	/*
	 * World Name Setting
	 */
	if (EDIT_TXT_WorldName->GetText().IsEmpty())
	{
		Request.DisplayName = FString("New World");
	}
	else
	{
		Request.DisplayName = EDIT_TXT_WorldName->GetText().ToString();
	}
	
	
	const int32 X = FCString::Atoi(*EDIT_TXT_Size_X->GetText().ToString());
	const int32 Y = FCString::Atoi(*EDIT_TXT_Size_Y->GetText().ToString());
	
	/*
	 * World Size Setting
	 */
	if (X <= 0 || Y <= 0)
	{
		Request.WorldSize= FIntPoint(1024, 1024);
	}
	else
	{
		Request.WorldSize= FIntPoint(X, Y);
	}
	
	
	
	
	UGameplayMessageSubsystem& MessageSubsystem = 
		UGameplayMessageSubsystem::Get(this);
	
	MessageSubsystem.BroadcastMessage(FallenEraGameplayTags::TAG_Event_World_CreateRequested, Request);
}
