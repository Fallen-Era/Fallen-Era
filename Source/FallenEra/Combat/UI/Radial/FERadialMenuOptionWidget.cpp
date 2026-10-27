#include "Combat/UI/Radial/FERadialMenuOptionWidget.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

void UFE_RadialMenuOptionWidget::UpdateItem(const FFE_RadialMenuItem& Item, int32 Index, int32 Count, bool bSelected,
	float WheelSize, float InnerRadius, float OuterRadius)
{
	SetIsFocusable(false);
	if (!Image_Sector || !Border_Content || Count <= 0 || WheelSize <= 0.0f) { return; }
	const FLinearColor Color = !Item.bEnabled ? DisabledColor : bSelected ? SelectedColor : NormalColor;
	if (UMaterialInstanceDynamic* Material = Image_Sector->GetDynamicMaterial())
	{
		Material->SetScalarParameterValue(TEXT("SectorStart"), UE_TWO_PI * Index / Count);
		Material->SetScalarParameterValue(TEXT("SectorAngle"), UE_TWO_PI / Count);
		Material->SetScalarParameterValue(TEXT("InnerRadius"), InnerRadius / WheelSize);
		Material->SetScalarParameterValue(TEXT("OuterRadius"), OuterRadius / WheelSize);
		Material->SetVectorParameterValue(TEXT("Tint"), Color);
	}
	const double Angle = UE_TWO_PI * (Index + 0.5) / Count;
	if (UCanvasPanelSlot* ContentSlot = Cast<UCanvasPanelSlot>(Border_Content->Slot))
	{
		ContentSlot->SetPosition(FVector2D(WheelSize * 0.5) + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * (InnerRadius + OuterRadius) * 0.5);
		ContentSlot->SetAlignment(FVector2D(0.5));
		ContentSlot->SetAutoSize(true);
	}
	if (Text_Label) { Text_Label->SetText(Item.Label); }
	if (Text_Secondary) { Text_Secondary->SetText(Item.SecondaryText); }
	if (Text_Detail) { Text_Detail->SetText(Item.DetailText); }
	if (Text_Marker)
	{
		Text_Marker->SetText(Item.MarkerText);
		Text_Marker->SetVisibility(Item.bMarked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Image_Icon)
	{
		UTexture2D* Icon = Item.Icon.Get();
		Image_Icon->SetBrushFromTexture(Icon);
		Image_Icon->SetVisibility(Icon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	Border_Content->SetRenderOpacity(Item.bEnabled ? 1.0f : 0.4f);
	OnItemUpdated(Item, bSelected);
}
