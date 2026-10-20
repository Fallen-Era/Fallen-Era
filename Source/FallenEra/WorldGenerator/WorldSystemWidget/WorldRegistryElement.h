// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldGenerator/Data/Payload.h"
#include "WorldRegistryElement.generated.h"

class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFocusInteraction, UUserWidget*)

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldRegistryElement : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Focused();
	void UnFocused();
	
	void SetWorldName(const FText& NewWorldName);
	void SetWorldInfo(const FText& NewWorldInfo);
	
	
	FOnFocusInteraction OnFocusInteraction;

	
	
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<class UButton> Btn_Interaction;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_WorldName;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_WorldInfo;
	
	FWorldRegistryData MetaData;
	
	friend class ULoadGameWidget;
	
	
private:
	UFUNCTION()
	void OnClickedInteraction();
	
	
	
	FButtonStyle NormalStyle;
	FButtonStyle FocusedStyle;
};
