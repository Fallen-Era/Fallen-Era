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

	
	if (EDIT_TXT_Size_X)
	{
		EDIT_TXT_Size_X->OnTextChanged.AddDynamic(this, &UNewGameWidget::OnTXTSizeXChanged);
		EDIT_TXT_Size_X->OnTextCommitted.AddDynamic(this, &UNewGameWidget::OnTXTSizeXCommitted);
	}
	
	if (EDIT_TXT_Size_Y)
	{
		EDIT_TXT_Size_Y->OnTextChanged.AddDynamic(this, &UNewGameWidget::OnTXTSizeYChanged);
		EDIT_TXT_Size_Y->OnTextCommitted.AddDynamic(this, &UNewGameWidget::OnTXTSizeYCommitted);
	}
	
	
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
	
	FVector2D Extent = FVector2D(512, 512);
	/*
	 * World Size Setting
	 */
	if (X <= 0 || Y <= 0)
	{
		Request.WorldBounds = FBox2D(-Extent, Extent);
	}
	else
	{
		Extent = FVector2D(X, Y);
		Extent *= 0.5;
		Request.WorldBounds = FBox2D(-Extent, Extent);
	}
	
	/*
	 * World Seed Setting
	 */
	Request.WorldSeed = FMath::Rand();
	
	if (EDIT_TXT_WorldSeed)
	{
		const FString SeedString = EDIT_TXT_WorldSeed->GetText().ToString().ToUpper();
	
		if (!SeedString.IsEmpty())
		{
			Request.WorldSeed = static_cast<int32>(
				FCrc::StrCrc32(*SeedString) & 0x7FFFFFFFu);
		}
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

void UNewGameWidget::ValidatePositiveIntegerInput(UEditableText* EditableTextRef, FText& PreviousValidValue, const FText& NewText)
{
	const FString Input = NewText.ToString();

	for (const TCHAR Char : Input)
	{
		if (!FChar::IsDigit(Char))
		{
			EditableTextRef->SetText(PreviousValidValue);
			return;
		}
	}

	int32 Value = 0;

	if (LexTryParseString(Value, *Input) && Value > 0)
	{
		PreviousValidValue = NewText;
		return;
	}

	EditableTextRef->SetText(PreviousValidValue);
}

void UNewGameWidget::SnapToHundred(
	UEditableText* EditableTextRef,
	FText& PreviousValidValue)
{
	if (!IsValid(EditableTextRef))
	{
		return;
	}

	int32 Value = 0;
	if (!LexTryParseString(Value, *PreviousValidValue.ToString()) ||
		Value <= 0)
	{
		return;
	}

	constexpr int32 Step = 100;
	Value = FMath::Max(Step, (Value / Step) * Step);

	const FString SnappedString = FString::FromInt(Value);
	PreviousValidValue = FText::FromString(SnappedString);

	if (EditableTextRef->GetText().ToString() != SnappedString)
	{
		EditableTextRef->SetText(PreviousValidValue);
	}
}

void UNewGameWidget::OnTXTSizeXChanged(const FText& Text)
{
	ValidatePositiveIntegerInput(EDIT_TXT_Size_X, ValidSizeX, Text);
	
}


void UNewGameWidget::OnTXTSizeYChanged(const FText& Text)
{
	ValidatePositiveIntegerInput(EDIT_TXT_Size_Y, ValidSizeY, Text);
	
}

void UNewGameWidget::OnTXTSizeXCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	SnapToHundred(EDIT_TXT_Size_X, ValidSizeX);
}

void UNewGameWidget::OnTXTSizeYCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	SnapToHundred(EDIT_TXT_Size_Y, ValidSizeY);
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

