#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FERadialMenuTestReceiver.generated.h"

class UFE_RadialMenuViewModel;
UCLASS(Transient)
class UFE_RadialMenuTestReceiver : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION() void ExecuteItem(FName ItemId);
	TWeakObjectPtr<UFE_RadialMenuViewModel> Menu;
	int32 ExecutionCount = 0;
	FName LastItem;
	bool bReentered = false;
};
