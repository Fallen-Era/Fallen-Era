#pragma once
#include "Combat/UI/Radial/FERadialMenuOptionWidget.h"
#include "FEAmmoOptionWidget.generated.h"

/** Existing ammo designer names are not requirements of the common radial entry. */
UCLASS(Abstract, Blueprintable)
class FALLENERA_API UFE_AmmoOptionWidget : public UFE_RadialMenuOptionWidget
{
	GENERATED_BODY()
public:
	virtual void UpdateItem(const FFE_RadialMenuItem& Item, int32 Index, int32 Count, bool bSelected,
		float WheelSize, float InnerRadius, float OuterRadius) override;
protected:
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_AmmoName;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Count;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Damage;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Loaded;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_AmmoIcon;
};
