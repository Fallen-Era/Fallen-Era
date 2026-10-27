#include "Combat/UI/FEAmmoOptionWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UFE_AmmoOptionWidget::UpdateItem(const FFE_RadialMenuItem& Item, int32 Index, int32 Count, bool bSelected,
	float WheelSize, float InnerRadius, float OuterRadius)
{
	Super::UpdateItem(Item, Index, Count, bSelected, WheelSize, InnerRadius, OuterRadius);
	if (Text_AmmoName) { Text_AmmoName->SetText(Item.Label); }
	if (Text_Count) { Text_Count->SetText(Item.SecondaryText); }
	if (Text_Damage) { Text_Damage->SetText(Item.DetailText); }
	if (Text_Loaded)
	{
		Text_Loaded->SetText(Item.MarkerText);
		Text_Loaded->SetVisibility(Item.bMarked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Image_AmmoIcon)
	{
		UTexture2D* Icon = Item.Icon.Get();
		Image_AmmoIcon->SetBrushFromTexture(Icon);
		Image_AmmoIcon->SetVisibility(Icon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
