#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Damage/FEHitZone.h"
#include "FEHitZoneMultiplierData.generated.h"

/** Damage tuning reusable independently of skeleton bone names. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_HitZoneMultiplierData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	TMap<EFE_HitZone, float> ZoneMultipliers;

	float GetDamageMultiplier(EFE_HitZone Zone) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
