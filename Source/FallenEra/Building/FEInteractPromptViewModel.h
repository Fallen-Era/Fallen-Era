// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Components/SlateWrapperTypes.h"
#include "FEInteractPromptViewModel.generated.h"

/** "E · 문 열기" 프롬프트. UFEInteractionComponent 가 0.1초마다 갱신 */
UCLASS(BlueprintType)
class FALLENERA_API UFEInteractPromptViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** 빈 텍스트면 숨김 */
	void SetPrompt(const FText& InText);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Interaction|UI")
	FText PromptText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "FallenEra|Interaction|UI")
	ESlateVisibility PromptVisibility = ESlateVisibility::Collapsed;
};