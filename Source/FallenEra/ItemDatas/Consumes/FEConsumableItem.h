#pragma once

#include "FEItemBase.h"
#include "FEConsumableItem.generated.h"

UCLASS(BlueprintType)
class UFEConsumableItem : public UFEItemBase
{
	GENERATED_BODY()
	
public:
	// ====================================================================
	// 섭취 효과
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable|Effeacts", meta = (Categories="Stat"))
	TMap<FGameplayTag,float> ConsumeEffects;
	
	// ====================================================================
	// 소모품 전용 사운드
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Visual|SFX")
	TSoftObjectPtr<USoundBase> ConsumeSound;
};
