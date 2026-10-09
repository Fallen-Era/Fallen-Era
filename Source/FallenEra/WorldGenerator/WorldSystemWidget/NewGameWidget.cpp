// Fill out your copyright notice in the Description page of Project Settings.


#include "NewGameWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"

#include "WorldGenerator/GameplayTag/WorldGameplayTag.h"
#include "GameFramework/GameplayMessageSubsystem.h"


void UNewGameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (Btn_WorldGen) Btn_WorldGen->OnClicked.AddDynamic(this, &UNewGameWidget::OnWorldGenerateClicked);
	if (Btn_Exit) Btn_Exit->OnClicked.AddDynamic(this, &UNewGameWidget::OnExitClicked);
	if (Btn_Randomize) Btn_Randomize->OnClicked.AddDynamic(this, &UNewGameWidget::OnRandomizeClicked);

	
	if (EDIT_TXT_Size_X) EDIT_TXT_Size_X->OnTextChanged.AddDynamic(this, &UNewGameWidget::OnTXTSizeXChanged);
	if (EDIT_TXT_Size_Y) EDIT_TXT_Size_Y->OnTextChanged.AddDynamic(this, &UNewGameWidget::OnTXTSizeYChanged);
	if (EDIT_TXT_WorldSeed) EDIT_TXT_WorldSeed->OnTextChanged.AddDynamic(this, &UNewGameWidget::OnTXTSeedChanged);
	
	
	PostUpdate();
}

void UNewGameWidget::PostUpdate()
{
	RandomizeSeed(FMath::Rand());
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
	
	MessageSubsystem.BroadcastMessage(WorldGameplayTag::TAG_Event_World_CreateRequested, Request);
}

void UNewGameWidget::OnExitClicked()
{
	OnNewGameExitClicked.Broadcast();
}

void UNewGameWidget::OnRandomizeClicked()
{
	RandomizeSeed(FMath::Rand());
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

void UNewGameWidget::OnTXTSeedChanged(const FText& Text)
{
	FString Input = Text.ToString().ToUpper();

	FString Filtered;
	Filtered.Reserve(8);

	for (const TCHAR Char : Input)
	{
		if (Char >= TEXT('A') && Char <= TEXT('Z'))
		{
			Filtered.AppendChar(Char);

			if (Filtered.Len() >= 8)
			{
				break;
			}
		}
	}

	if (Filtered != Input)
	{
		EDIT_TXT_WorldSeed->SetText(FText::FromString(Filtered));
	}
}

void UNewGameWidget::RandomizeSeed(const int32 RandSeed)
{
	if (!EDIT_TXT_WorldSeed)
	{
		return;
	}

	const FRandomStream Stream(RandSeed);

	FString Seed;
	Seed.Reserve(8);

	for (int32 Index = 0; Index < 8; ++Index)
	{
		Seed.AppendChar(
			static_cast<TCHAR>(Stream.RandRange(TEXT('A'), TEXT('Z'))));
	}

	EDIT_TXT_WorldSeed->SetText(FText::FromString(Seed));
}

