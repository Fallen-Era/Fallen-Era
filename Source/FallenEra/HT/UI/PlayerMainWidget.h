#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerMainWidget.generated.h"

class UFE_PlayerCrossHairWidget;
class UFE_PlayerStatusWidget;

/** Root player HUD that owns the status and crosshair widgets. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_PlayerMainWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UFE_PlayerCrossHairWidget> W_PlayerCrossHair;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UFE_PlayerStatusWidget> W_PlayerStatus;
};
