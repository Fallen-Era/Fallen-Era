// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldGenerator/Data/Payload.h"
#include "LoadGameWidget.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadGameExitCliked);

class UButton;
class UVerticalBox;
class UWorldRegistryElement;

/**
 * 
 */
UCLASS()
class FALLENERA_API ULoadGameWidget : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UFUNCTION()
	void UpdateRegistryElements();
	

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_Load;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_Delete;
	
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_Exit;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> VB_ElementList;
	
	
	
	UFUNCTION()
	void ClearElementsList();
	
	
	void SetFocus(UUserWidget* Sender);
	
	
	
protected:
	virtual void NativeConstruct();



public:
	UPROPERTY(BlueprintAssignable)
	FOnLoadGameExitCliked OnLoadGameExitCliked;
	
private:
	TSubclassOf<UWorldRegistryElement> WorldElementClass;
	
	UPROPERTY(Transient)
	TArray<UWorldRegistryElement*> WorldElements;
	
	UPROPERTY(Transient)
	TObjectPtr<UWorldRegistryElement> FocusElement;
	
	
	UFUNCTION()
	void OnEixtClicked();
	
	UFUNCTION()
	void OnLoadClicked();
	
	UFUNCTION()
	void OnDeleteClicked();
};
