// Fill out your copyright notice in the Description page of Project Settings.


#include "NewGameWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"

#include "FallenEra/AbilitySystem/FallenEraGameplayTags.h"
#include "GameFramework/GameplayMessageSubsystem.h"


void UNewGameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (Btn_WorldGen)
	{
		Btn_WorldGen->OnClicked.AddDynamic(this, &UNewGameWidget::OnWorldGenerateClicked);
	}
	
	
	EDIT_TXT_Size_X->OnTextChanged.AddDynamic(this, &UNewGameWidget::OnTXTSizeXChanged);
	EDIT_TXT_Size_Y->OnTextChanged.AddDynamic(this, &UNewGameWidget::OnTXTSizeYChanged);
	
}

void UNewGameWidget::OnWorldGenerateClicked()
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

void UNewGameWidget::OnTXTSizeXChanged(const FText& Text)
{
	const FString Input = Text.ToString();

	for (const TCHAR Char : Input)
	{
		if (!FChar::IsDigit(Char))
		{
			EDIT_TXT_Size_X->SetText(ValidSizeX);
			return;
		}
	}

	int32 Value = 0;

	if (LexTryParseString(Value, *Input) && Value > 0)
	{
		ValidSizeX = Text;
		return;
	}

	EDIT_TXT_Size_X->SetText(ValidSizeX);
}

void UNewGameWidget::OnTXTSizeYChanged(const FText& Text)
{
	const FString Input = Text.ToString();

	for (const TCHAR Char : Input)
	{
		if (!FChar::IsDigit(Char))
		{
			EDIT_TXT_Size_Y->SetText(ValidSizeY);
			return;
		}
	}

	int32 Value = 0;

	if (LexTryParseString(Value, *Input) && Value > 0)
	{
		ValidSizeY = Text;
		return;
	}

	EDIT_TXT_Size_Y->SetText(ValidSizeY);
}

