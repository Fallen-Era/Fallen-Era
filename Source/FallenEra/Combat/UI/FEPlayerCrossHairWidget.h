#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FEPlayerCrossHairWidget.generated.h"

class UFE_EquipmentComponent;
class UFE_CrossHairWidget;
class UOverlay;
class UFE_WeaponItemData;

/** HUD host that replaces the active weapon-specific crosshair widget. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_PlayerCrossHairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="Player Crosshair")
	void RefreshFromCharacter();

	UFUNCTION(BlueprintPure, Category="Player Crosshair")
	UFE_CrossHairWidget* GetActiveCrosshairWidget() const { return ActiveCrosshairWidget; }

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UOverlay> Overlay_CrossHairContainer;

private:
	void BindToEquipment(UFE_EquipmentComponent* Equipment);
	void UnbindFromEquipment();
	void OnWeaponChanged(const UFE_WeaponItemData* WeaponData);

	TWeakObjectPtr<UFE_EquipmentComponent> BoundEquipment;
	TWeakObjectPtr<const UFE_WeaponItemData> CachedWeaponData;

	UPROPERTY(Transient)
	TObjectPtr<UFE_CrossHairWidget> ActiveCrosshairWidget;

	FDelegateHandle WeaponChangedHandle;
};
