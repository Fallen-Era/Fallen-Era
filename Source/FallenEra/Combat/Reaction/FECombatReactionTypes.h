#pragma once

#include "CoreMinimal.h"
#include "FECombatReactionTypes.generated.h"

/** Ordered strength, not a damage amount. Equal-or-weaker hits can be resisted by an attack. */
UENUM(BlueprintType)
enum class EFE_StaggerStrength : uint8
{
	None,
	Light,
	Heavy
};

/** Server-side snapshot evaluated before cancellation removes the current attack's protection. */
struct FFE_DamageReactionDecision
{
	bool bInterruptAttack = false;
	bool bPlayHitReaction = false;
	bool bApplyStun = false;
};
