#pragma once
#include "FEItemBase.h"
#include "FEItemType.h"
#include "FEResourceItem.generated.h"

UCLASS(BlueprintType)
class FALLENERA_API UFEResourceItem : public UFEItemBase
{
	GENERATED_BODY()

public:
	UFEResourceItem()
	{
		// 자원은 기본적으로 다량 중첩이 가능하므로 기본 스택을 99로 설정
		MaxStack = 99;
	}

	/** 장비 등 제작 시 결과물에 기여할 스탯 보정치 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource|Crafting")
	TArray<FCraftingModifier> CraftingModifiers;
};
