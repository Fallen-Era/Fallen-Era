#pragma once

#include "FallenEraItemBase.h"
#include "FallenEraConsumableItem.generated.h"

UCLASS(BlueprintType)
class UFallenEraConsumableItem : public UFallenEraItemBase
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
