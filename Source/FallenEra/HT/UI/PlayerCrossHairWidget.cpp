#include "HT/UI/PlayerCrossHairWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "HT/Component/EquipmentComponent.h"
#include "HT/Weapon/WeaponItemData.h"
#include "Styling/SlateBrush.h"

void UFE_PlayerCrossHairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromCharacter();
}

void UFE_PlayerCrossHairWidget::NativeDestruct()
{
	UnbindFromEquipment();
	Super::NativeDestruct();
}

void UFE_PlayerCrossHairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BoundEquipment.IsValid())
	{
		RefreshFromCharacter();
	}
}

void UFE_PlayerCrossHairWidget::RefreshFromCharacter()
{
	APawn* Pawn = GetOwningPlayerPawn();
	UFE_EquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UFE_EquipmentComponent>() : nullptr;
	if (!Equipment)
	{
		UnbindFromEquipment();
		OnWeaponChanged(nullptr);
		return;
	}

	BindToEquipment(Equipment);
	OnWeaponChanged(Equipment->GetCurrentWeaponData());
}

void UFE_PlayerCrossHairWidget::BindToEquipment(UFE_EquipmentComponent* Equipment)
{
	if (BoundEquipment.Get() == Equipment)
	{
		return;
	}

	UnbindFromEquipment();
	BoundEquipment = Equipment;
	WeaponChangedHandle = Equipment->OnWeaponChanged().AddUObject(this, &UFE_PlayerCrossHairWidget::OnWeaponChanged);
}

void UFE_PlayerCrossHairWidget::UnbindFromEquipment()
{
	if (UFE_EquipmentComponent* Equipment = BoundEquipment.Get(); Equipment && WeaponChangedHandle.IsValid())
	{
		Equipment->OnWeaponChanged().Remove(WeaponChangedHandle);
	}

	WeaponChangedHandle.Reset();
	BoundEquipment.Reset();
}

void UFE_PlayerCrossHairWidget::OnWeaponChanged(const UFE_WeaponItemData* WeaponData)
{
	if (!Image_CrossHair)
	{
		return;
	}

	if (CachedWeaponData.Get() == WeaponData)
	{
		return;
	}

	CachedWeaponData = WeaponData;
	UTexture2D* CrosshairTexture = WeaponData ? WeaponData->CrosshairTexture.Get() : nullptr;
	if (CrosshairTexture)
	{
		Image_CrossHair->SetBrushFromTexture(CrosshairTexture, true);
		Image_CrossHair->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Image_CrossHair->SetBrush(FSlateBrush());
		Image_CrossHair->SetVisibility(ESlateVisibility::Collapsed);
	}
}
