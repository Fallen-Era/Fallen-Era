// Fill out your copyright notice in the Description page of Project Settings.


#include "LoadGameWidget.h"

#include "Components/VerticalBox.h"
#include "Components/Button.h"

#include "WorldGenerator/WorldSystemWidget/WorldElement.h"


void ULoadGameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	
	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddDynamic(this, &ULoadGameWidget::OnEixtClicked);
	}
	
}

void ULoadGameWidget::ClearElementsList()
{
	VB_ElementList->ClearChildren();
	WorldElements.Reset();
}


void ULoadGameWidget::UpdateRegistryElements(TArray<FWorldRegistryData>& Worlds)
{
	ClearElementsList();

	if (!WorldElementClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%s : WorldElementClass is not assigned."), *GetClass()->GetName(), TEXT(__FUNCTION__));
		return;
	}

	for (const FWorldRegistryData& World : Worlds)
	{
		UWorldElement* Element =
			CreateWidget<UWorldElement>(
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

		VB_ElementList->AddChild(Element);
		WorldElements.Add(Element);
	}
}


void ULoadGameWidget::OnEixtClicked()
{
	OnLoadGameExitCliked.Broadcast();
}
