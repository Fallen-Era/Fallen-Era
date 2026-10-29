#include "Combat/UI/FEPlayerMainWidget.h"

#include "Combat/UI/FEPlayerCrossHairWidget.h"
#include "Combat/UI/FEPlayerStatusWidget.h"

void UFE_PlayerMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (W_PlayerCrossHair)
	{
		W_PlayerCrossHair->RefreshFromCharacter();
	}
	if (W_PlayerStatus)
	{
		W_PlayerStatus->RefreshFromPlayerState();
	}
}
