#pragma once
#include "Combat/UI/Radial/FERadialMenuWidget.h"
#include "FEAmmoRadialWidget.generated.h"

class UFE_AmmoSelectionViewModel;

/** Compatibility adapter for the existing ammo designer WBP. */
UCLASS(Abstract, Blueprintable)
class FALLENERA_API UFE_AmmoRadialWidget : public UFE_RadialMenuWidget
{
	GENERATED_BODY()
public:
	bool SetViewModel(UFE_AmmoSelectionViewModel* InModel);
protected:
	virtual void UpdateSelectedText(const FText& Text) override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SelectedAmmo;
};
