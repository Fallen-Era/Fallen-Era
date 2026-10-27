#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Combat/Item/FEEquipmentInstanceData.h"
#include "FEArmorItemData.generated.h"

USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ArmorTemplateData
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Identity")
	FGameplayTag ItemTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Identity")
	FGameplayTag EquipmentSlotTag;
	/** Generator baseline; equipped stats are read from Instance, not this shared asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Stats")
	FFE_ArmorStat ArmorStat;
};

UCLASS(BlueprintType)
class FALLENERA_API UFE_ArmorItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Template")
	FFE_ArmorTemplateData Template;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Instance")
	FFE_ArmorInstanceData Instance;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
private:
	UPROPERTY()
	int32 ItemDataVersion = 0;
	// Load-only legacy fields, not a runtime fallback.
	UPROPERTY(meta=(DeprecatedProperty))
	FGameplayTag ItemTag;
	UPROPERTY(meta=(DeprecatedProperty))
	FGameplayTag EquipmentSlotTag;
	UPROPERTY(meta=(DeprecatedProperty))
	FFE_ArmorStat ArmorStat;
};
