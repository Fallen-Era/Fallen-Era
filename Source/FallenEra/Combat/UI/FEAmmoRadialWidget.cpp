#include "Combat/UI/FEAmmoRadialWidget.h"
#include "Combat/UI/FEAmmoSelectionViewModel.h"
#include "Components/TextBlock.h"

bool UFE_AmmoRadialWidget::SetViewModel(UFE_AmmoSelectionViewModel* InModel)
{
	if (InModel)
	{
		InModel->SetMenuText(NSLOCTEXT("FEAmmo", "Title", "탄약 선택"), NSLOCTEXT("FEAmmo", "Confirm", "R 해제: 장전"),
			NSLOCTEXT("FEAmmo", "Cancel", "R 해제: 취소"), NSLOCTEXT("FEAmmo", "Empty", "호환 탄약 없음"));
		InModel->SyncMenuItems();
	}
	return SetMenuModel(InModel);
}

void UFE_AmmoRadialWidget::UpdateSelectedText(const FText& Text)
{
	Super::UpdateSelectedText(Text);
	if (Text_SelectedAmmo) { Text_SelectedAmmo->SetText(Text); }
}
