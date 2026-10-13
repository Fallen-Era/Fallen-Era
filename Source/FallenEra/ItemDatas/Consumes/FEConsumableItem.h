#pragma once

#include "FEItemBase.h"
#include "FEConsumableItem.generated.h"

UCLASS(BlueprintType)
class FALLENERA_API UFEConsumableItem : public UFEItemBase
{
	GENERATED_BODY()
	
public:
	UFEConsumableItem()
	{
		// 소모품(물약, 음식 등)은 기본 중첩을 20으로 설정
		MaxStack = 20;
	}

	// ====================================================================
	// 섭취 효과
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Consumable|Effects", meta = (Categories="Stat"))
	TMap<FGameplayTag,float> ConsumeEffects;
	
	// ====================================================================
	// 소모품 전용 사운드
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Visual|SFX")
	TSoftObjectPtr<USoundBase> ConsumeSound;
};
