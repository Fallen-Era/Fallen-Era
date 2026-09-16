#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerCrossHairWidget.generated.h"

class AFE_CombatCharacter;
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
	void BindToCharacter(AFE_CombatCharacter* CombatCharacter);
	void UnbindFromCharacter();
	void OnWeaponChanged(const UFE_WeaponItemData* WeaponData);

	TWeakObjectPtr<AFE_CombatCharacter> BoundCharacter;
	TWeakObjectPtr<const UFE_WeaponItemData> CachedWeaponData;
	FDelegateHandle WeaponChangedHandle;
};
