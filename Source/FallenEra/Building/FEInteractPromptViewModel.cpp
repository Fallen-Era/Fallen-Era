// Fill out your copyright notice in the Description page of Project Settings.

#include "FEInteractPromptViewModel.h"

void UFEInteractPromptViewModel::SetPrompt(const FText& InText)
{
	UE_MVVM_SET_PROPERTY_VALUE(PromptText, InText);
	UE_MVVM_SET_PROPERTY_VALUE(PromptVisibility, InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}