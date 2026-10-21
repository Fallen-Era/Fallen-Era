#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/UI/Radial/FERadialMenuViewModel.h"
#include "FERadialMenuOptionWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;

/** Designer widgets own layout; the sector material and content position follow the current item count. */
UCLASS(Abstract, Blueprintable)
class FALLENERA_API UFE_RadialMenuOptionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void UpdateItem(const FFE_RadialMenuItem& Item, int32 Index, int32 Count, bool bSelected,
		float WheelSize, float InnerRadius, float OuterRadius);
protected:
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Image_Sector;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> Border_Content;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Label;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Secondary;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Detail;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Marker;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_Icon;
	UPROPERTY(EditDefaultsOnly, Category="Radial Menu") FLinearColor NormalColor = FLinearColor(0.055f, 0.075f, 0.1f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category="Radial Menu") FLinearColor SelectedColor = FLinearColor(0.08f, 0.3f, 0.5f, 0.98f);
	UPROPERTY(EditDefaultsOnly, Category="Radial Menu") FLinearColor DisabledColor = FLinearColor(0.035f, 0.035f, 0.035f, 0.9f);
	UFUNCTION(BlueprintImplementableEvent, Category="Radial Menu") void OnItemUpdated(const FFE_RadialMenuItem& Item, bool bSelected);
};
