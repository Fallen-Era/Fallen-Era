#include "HT/UI/PlayerMainWidget.h"

#include "HT/UI/PlayerCrossHairWidget.h"
#include "HT/UI/PlayerStatusWidget.h"

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
