#include "HT/UI/PlayerCrossHairWidget.h"

#include "Components/Overlay.h"
#include "GameFramework/Pawn.h"
#include "HT/Component/EquipmentComponent.h"
#include "HT/UI/CrossHairWidget.h"
#include "HT/Weapon/WeaponItemData.h"

void UFE_PlayerCrossHairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromCharacter();
}

void UFE_PlayerCrossHairWidget::NativeDestruct()
{
	UnbindFromEquipment();
	CachedWeaponData.Reset();
	ActiveCrosshairWidget = nullptr;
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
	if (!Overlay_CrossHairContainer)
	{
		return;
	}

	if (CachedWeaponData.Get() == WeaponData)
	{
		return;
	}

	CachedWeaponData = WeaponData;
	Overlay_CrossHairContainer->ClearChildren();
	ActiveCrosshairWidget = nullptr;
	TSubclassOf<UFE_CrossHairWidget> WidgetClass = WeaponData
		? WeaponData->CrosshairWidgetClass.Get()
		: nullptr;
	if (WidgetClass && GetOwningPlayer())
	{
		ActiveCrosshairWidget = CreateWidget<UFE_CrossHairWidget>(GetOwningPlayer(), WidgetClass);
		if (ActiveCrosshairWidget)
		{
			Overlay_CrossHairContainer->AddChildToOverlay(ActiveCrosshairWidget);
		}
	}
}
