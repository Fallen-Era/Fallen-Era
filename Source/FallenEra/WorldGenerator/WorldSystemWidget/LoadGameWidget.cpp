// Fill out your copyright notice in the Description page of Project Settings.


#include "LoadGameWidget.h"

#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "WorldGenerator/Subsystem/WorldRegistrySubsystem.h"
#include "WorldGenerator/Settings/WorldGeneratorSettings.h"

#include "WorldGenerator/WorldSystemWidget/WorldRegistryElement.h"


void ULoadGameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	
	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddUniqueDynamic(this, &ULoadGameWidget::OnEixtClicked);
	}
	
	if (Btn_Load)
	{
		Btn_Load->OnClicked.AddUniqueDynamic(this, &ULoadGameWidget::OnLoadClicked);
	}
	
	if (Btn_Delete)
	{
		Btn_Delete->OnClicked.AddUniqueDynamic(this, &ULoadGameWidget::OnDeleteClicked);
	}
	
	const UWorldGeneratorSettings* Settings = GetDefault<UWorldGeneratorSettings>(); 
	
	
	WorldElementClass = Settings->WorldElementClass;
}

void ULoadGameWidget::ClearElementsList()
{
	FocusElement = nullptr;
	for (auto Element : WorldElements)
	{
		Element->OnFocusInteraction.RemoveAll(this);
	}
	VB_ElementList->ClearChildren();
	WorldElements.Reset();
}

void ULoadGameWidget::SetFocus(UUserWidget* Sender)
{
	UWorldRegistryElement* NewFocus = Cast<UWorldRegistryElement>(Sender);

	if (!NewFocus)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s::%s : Failed to cast UWorldRegistryElement."),
			*GetClass()->GetName(),
			TEXT(__FUNCTION__)
		);
		return;
	}

	if (FocusElement == NewFocus)
	{
		return;
	}

	if (FocusElement)
	{
		FocusElement->UnFocused();
	}

	FocusElement = NewFocus;
	FocusElement->Focused();
}


void ULoadGameWidget::UpdateRegistryElements()
{
	ClearElementsList();
	
	UWorldRegistrySubsystem* WorldRegistrySubsystem = GetGameInstance()->GetSubsystem<UWorldRegistrySubsystem>();
	
	TArray<FWorldRegistryData>& Worlds = WorldRegistrySubsystem->GetWorldRegistryList();
	
	if (!WorldElementClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : WorldElementClass is not assigned."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}

	for (const FWorldRegistryData& World : Worlds)
	{
		UWorldRegistryElement* Element =
			CreateWidget<UWorldRegistryElement>(
				GetOwningPlayer(),
				WorldElementClass
			);

		if (!Element)
		{
			continue;
		}

		// World Name
		Element->SetWorldName(
			FText::FromString(World.DisplayName)
		);

		// Difficulty
		const FText DifficultyText =
			UEnum::GetDisplayValueAsText(World.Diffculty);

		// World Info
		const FText WorldInfo = FText::Format(
			NSLOCTEXT(
				"WorldLoad",
				"WorldInfoFormat",
				"World Size : {0} x {1}\nWorld Difficulty : {2}"
			),
			FText::AsNumber(World.WorldSize.X),
			FText::AsNumber(World.WorldSize.Y),
			DifficultyText
		);

		Element->SetWorldInfo(WorldInfo);
		Element->OnFocusInteraction.AddUObject(this, &ULoadGameWidget::SetFocus);
		Element->MetaData = World;

		VB_ElementList->AddChild(Element);
		WorldElements.Add(Element);
	}
}


void ULoadGameWidget::OnEixtClicked()
{
	OnLoadGameExitCliked.Broadcast();
}

void ULoadGameWidget::OnLoadClicked()
{
	if (!FocusElement || !WorldElements.Contains(FocusElement.Get()))
	{
		return;
	}

	UWorldRegistrySubsystem* WorldRegistrySubsystem = GetGameInstance()->GetSubsystem<UWorldRegistrySubsystem>();
	
	const FString SlotName = FocusElement->MetaData.ProfileSlotName;
	
	WorldRegistrySubsystem->LoadWorld(SlotName);
}

void ULoadGameWidget::OnDeleteClicked()
{
	if (!FocusElement || !WorldElements.Contains(FocusElement.Get()))
	{
		return;
	}

	UWorldRegistrySubsystem* WorldRegistrySubsystem = GetGameInstance()->GetSubsystem<UWorldRegistrySubsystem>();

	
	WorldRegistrySubsystem->RemoveRegistry(FocusElement->MetaData);
	
	UpdateRegistryElements();
}
