#pragma once

#include "CoreMinimal.h"
#include "Combat/UI/CrossHairWidget.h"
#include "BowCrossHairWidget.generated.h"

class UFE_CombatComponent;
class UImage;

/** Bow crosshair whose charge reticle contracts from 1.0 to 0.35. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_BowCrossHairWidget : public UFE_CrossHairWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="FallenEra|Bow Crosshair")
	void SetChargeAlpha(float ChargeAlpha);

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> Image_ChargeReticle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Bow Crosshair", meta=(ClampMin="0.01"))
	float FullyChargedScale = 0.35f;

private:
	void TryBindCombatComponent();
	void UnbindCombatComponent();

	TWeakObjectPtr<UFE_CombatComponent> BoundCombatComponent;
	FDelegateHandle ChargeChangedHandle;
};
