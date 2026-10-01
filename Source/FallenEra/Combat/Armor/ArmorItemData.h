#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArmorItemData.generated.h"

USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ArmorStat
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Stats", meta=(ClampMin="0"))
	float Defense = 0.0f;

	/** Flat reduction, in the same velocity units as attack KnockbackAmount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Stats", meta=(ClampMin="0"))
	float KnockbackResistance = 0.0f;
};

/** Shared immutable definition. Durability and upgrades belong to item instances. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_ArmorItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Identity")
	FGameplayTag ItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Identity")
	FGameplayTag EquipmentSlotTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor|Stats")
	FFE_ArmorStat ArmorStat;
};
