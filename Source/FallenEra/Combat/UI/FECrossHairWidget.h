#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FECrossHairWidget.generated.h"

class UImage;

/** Base class for a weapon-specific crosshair WBP. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_CrossHairWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Bind this to the normal crosshair image in every derived WBP. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_CrossHair;
};
