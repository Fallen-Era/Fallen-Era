#pragma once

#include "CoreMinimal.h"
#include "FEHitZone.generated.h"

UENUM(BlueprintType)
enum class EFE_HitZone : uint8
{
	Default UMETA(DisplayName="기본"),
	Head UMETA(DisplayName="머리"),
	Torso UMETA(DisplayName="몸통"),
	Arms UMETA(DisplayName="팔"),
	Legs UMETA(DisplayName="다리")
};
