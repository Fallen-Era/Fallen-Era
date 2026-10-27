#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "FEProceduralHitReactionSettings.generated.h"

class UHitReactProfile;

/** Cosmetic settings only. Damage, stun and attack interruption remain server-authoritative. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ProceduralHitReactionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Physical Reaction")
	bool bEnabled = true;

	/** Preloaded with the selected AI preset; never synchronously loaded on a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Physical Reaction")
	TSoftObjectPtr<UHitReactProfile> Profile = TSoftObjectPtr<UHitReactProfile>(FSoftObjectPath(
		TEXT("/Game/Combat/DataAssets/HitReaction/HRP_Enemy_Physical.HRP_Enemy_Physical")));

	/** Velocity change in cm/s, independent of individual Physics Asset body masses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Physical Reaction", meta=(ClampMin="0.0"))
	float ImpulseStrength = 150.0f;

	/** Zero disables the distance limit. Dedicated servers never simulate these reactions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Physical Reaction", meta=(ClampMin="0.0", Units="cm"))
	float MaxDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Physical Reaction")
	bool bRequireRecentlyRendered = true;
};

/** Compact multicast payload, not a replicated physics pose or an AttackData UObject. */
USTRUCT()
struct FALLENERA_API FFE_ProceduralHitReactionEvent
{
	GENERATED_BODY()

	UPROPERTY() FName HitBone;
	UPROPERTY() FVector_NetQuantizeNormal Direction = FVector::ZeroVector;
	UPROPERTY() bool bInterruptAttack = false;
};
