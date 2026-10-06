#include "Combat/UI/FEBowCrossHairWidget.h"

#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "Combat/Component/FECombatComponent.h"

void UFE_BowCrossHairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetChargeAlpha(0.0f);
	TryBindCombatComponent();
}

void UFE_BowCrossHairWidget::NativeDestruct()
{
	UnbindCombatComponent();
	Super::NativeDestruct();
}

void UFE_BowCrossHairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BoundCombatComponent.IsValid())
	{
		TryBindCombatComponent();
	}
}

void UFE_BowCrossHairWidget::SetChargeAlpha(float ChargeAlpha)
{
	if (!Image_ChargeReticle)
	{
		return;
	}
	const float Scale = FMath::Lerp(1.0f, FullyChargedScale, FMath::Clamp(ChargeAlpha, 0.0f, 1.0f));
	Image_ChargeReticle->SetRenderScale(FVector2D(Scale));
}

void UFE_BowCrossHairWidget::TryBindCombatComponent()
{
	APawn* Pawn = GetOwningPlayerPawn();
	UFE_CombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<UFE_CombatComponent>() : nullptr;
	if (!Combat || BoundCombatComponent.Get() == Combat)
	{
		return;
	}
	UnbindCombatComponent();
	BoundCombatComponent = Combat;
	ChargeChangedHandle = Combat->OnBowChargeChanged().AddUObject(this, &UFE_BowCrossHairWidget::SetChargeAlpha);
	SetChargeAlpha(Combat->GetBowChargeAlpha());
}

void UFE_BowCrossHairWidget::UnbindCombatComponent()
{
	if (UFE_CombatComponent* Combat = BoundCombatComponent.Get(); Combat && ChargeChangedHandle.IsValid())
	{
		Combat->OnBowChargeChanged().Remove(ChargeChangedHandle);
	}
	ChargeChangedHandle.Reset();
	BoundCombatComponent.Reset();
}
