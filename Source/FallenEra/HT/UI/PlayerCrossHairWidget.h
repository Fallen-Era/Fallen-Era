#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerCrossHairWidget.generated.h"

class UFE_EquipmentComponent;
class UImage;
class UTexture2D;
class UFE_WeaponItemData;

/** Displays the crosshair texture configured by the currently equipped weapon. */
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

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> Image_CrossHair;

private:
	void BindToEquipment(UFE_EquipmentComponent* Equipment);
	void UnbindFromEquipment();
	void OnWeaponChanged(const UFE_WeaponItemData* WeaponData);

	TWeakObjectPtr<UFE_EquipmentComponent> BoundEquipment;
	TWeakObjectPtr<const UFE_WeaponItemData> CachedWeaponData;
	FDelegateHandle WeaponChangedHandle;
};
