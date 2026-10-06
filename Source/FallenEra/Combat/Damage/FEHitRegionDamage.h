#pragma once

#include "CoreMinimal.h"
#include "FEHitRegionDamage.generated.h"

/** Geometry does not determine damage type: a melee sphere sweep is still a direct hit. */
UENUM(BlueprintType)
enum class EFE_DamageHitType : uint8
{
	Direct,
	Area
};
