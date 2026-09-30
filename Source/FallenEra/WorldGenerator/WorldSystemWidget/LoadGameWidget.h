// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldGenerator/Data/Payload.h"
#include "LoadGameWidget.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadGameExitCliked);

class UButton;
class UVerticalBox;
class UWorldElement;

/**
 * 
 */
UCLASS()
class FALLENERA_API ULoadGameWidget : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UFUNCTION()
	void UpdateRegistryElements(TArray<FWorldRegistryData>& Worlds);
	

protected:
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_Exit;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> VB_ElementList;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UWorldElement> WorldElementClass;
	
	UFUNCTION()
	void ClearElementsList();
	
	
	
protected:
	virtual void NativeConstruct();



public:
	UPROPERTY(BlueprintAssignable)
	FOnLoadGameExitCliked OnLoadGameExitCliked;
	
private:
	UPROPERTY(Transient)
	TArray<UWorldElement*> WorldElements;
	
	UPROPERTY(Transient)
	TObjectPtr<UWorldElement> FocusElement;
	
	
	UFUNCTION()
	void OnEixtClicked();
};
