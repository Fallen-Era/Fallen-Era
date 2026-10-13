#pragma once
#include "FEItemBase.h"
#include "FEResourceItem.generated.h"

UCLASS(BlueprintType)
class UFEResourceItem : public UFEItemBase
{
	GENERATED_BODY()
public:
	// TODO : 추후 확장성을 고려, 현재 부모 데이터만으로 충분
};
