#pragma once

#include "FallenEraEquipmentItem.h"
#include "FallenEraWeaponItem.generated.h"

UCLASS(BlueprintType)
class FALLENERA_API UFallenEraWeaponItem : public UFallenEraEquipmentItem
{
	GENERATED_BODY()
public:
	// ====================================================================
	// 특수 전투 효과 확률 (상태이상 부여)
	// ====================================================================
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon|StatusEffects", meta=(Categories="Stat.Special"))
	TMap<FGameplayTag,float> StatEffectChances;
	
	// ====================================================================
	// 무기 전용 시청각 이펙트
	// ====================================================================
	
	// 무기 휘두르는 소리 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Visual|SFX")
	TSoftObjectPtr<class USoundBase> SwingSound;
	
	// 적을 타격했을 때 나는 소리 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Visual|SFX")
	TSoftObjectPtr<class USoundBase> HitSound;
};
