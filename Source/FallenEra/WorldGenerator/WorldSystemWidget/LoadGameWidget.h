// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadGameWidget.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadGameExitCliked);

class UButton;

/**
 * 
 */
UCLASS()
class FALLENERA_API ULoadGameWidget : public UUserWidget
{
	GENERATED_BODY()
	

protected:
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Btn_Exit;
	
protected:
	virtual void NativeConstruct();

public:
	UPROPERTY(BlueprintAssignable)
	FOnLoadGameExitCliked OnLoadGameExitCliked;
	
private:
	UFUNCTION()
	void OnEixtClicked();
};
