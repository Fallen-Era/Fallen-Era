#include "HT/UI/PlayerCrossHairWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "HT/Character/CombatCharacter.h"
#include "HT/Weapon/WeaponItemData.h"
#include "Styling/SlateBrush.h"

void UFE_PlayerCrossHairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromCharacter();
}

void UFE_PlayerCrossHairWidget::NativeDestruct()
{
	UnbindFromCharacter();
	Super::NativeDestruct();
}

void UFE_PlayerCrossHairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BoundCharacter.IsValid())
	{
		RefreshFromCharacter();
	}
}

void UFE_PlayerCrossHairWidget::RefreshFromCharacter()
{
	APawn* Pawn = GetOwningPlayerPawn();
	AFE_CombatCharacter* CombatCharacter = Cast<AFE_CombatCharacter>(Pawn);
	if (!CombatCharacter)
	{
		OnWeaponChanged(nullptr);
		return;
	}

	BindToCharacter(CombatCharacter);
	OnWeaponChanged(CombatCharacter->GetCurrentWeaponData());
}

void UFE_PlayerCrossHairWidget::BindToCharacter(AFE_CombatCharacter* CombatCharacter)
{
	if (BoundCharacter.Get() == CombatCharacter)
	{
		return;
	}

	UnbindFromCharacter();
	BoundCharacter = CombatCharacter;
	WeaponChangedHandle = CombatCharacter->OnWeaponChanged().AddUObject(this, &UFE_PlayerCrossHairWidget::OnWeaponChanged);
}

void UFE_PlayerCrossHairWidget::UnbindFromCharacter()
{
	if (AFE_CombatCharacter* CombatCharacter = BoundCharacter.Get(); CombatCharacter && WeaponChangedHandle.IsValid())
	{
		CombatCharacter->OnWeaponChanged().Remove(WeaponChangedHandle);
	}

	WeaponChangedHandle.Reset();
	BoundCharacter.Reset();
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
	UTexture2D* CrosshairTexture = WeaponData ? WeaponData->CrosshairTexture.LoadSynchronous() : nullptr;
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
