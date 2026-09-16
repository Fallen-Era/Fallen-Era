#pragma once

#include "FallenEraItemBase.h"
#include "FallenEraItemType.h"
#include "FallenEraEquipmentItem.generated.h"


UCLASS(Abstract, BlueprintType)
class FALLENERA_API UFallenEraEquipmentItem : public UFallenEraItemBase
{
	GENERATED_BODY()
	
public:
	// 장비 장착 슬롯
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Base")
	EEquipSlot EquipSlot;
	
	// ====================================================================
	// 내구도 및 관리 수치 
	// ====================================================================
	
	// 최대 내구도 (Stat_Durability_Max)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Durability")
	float MaxDurability = 100.0f;
	
	// 내구도 감소 배율 (Stat_Durability_DecreaseRate) - 닳는 속도 조절
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Durability")
	float DurabilityDecreaseRate = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Durability")
	float RepairCostMultiplier = 1.0f;
	
	// ====================================================================
	// 스탯 모디파이어 (방어력, 최대 체력 증가 등 캐릭터에게 주는 수치)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Stats",meta =(Categories="Stat"))
	TMap<FGameplayTag, float> StatModifier;
	
};
