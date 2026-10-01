#include "Combat/UI/PlayerMainWidget.h"

#include "Combat/UI/PlayerCrossHairWidget.h"
#include "Combat/UI/PlayerStatusWidget.h"

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
