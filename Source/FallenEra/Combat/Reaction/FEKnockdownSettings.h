#pragma once

#include "CoreMinimal.h"
#include "FEKnockdownSettings.generated.h"

class UAnimMontage;

/** Gameplay knockdown, independent of cosmetic limb reactions and stun immunity. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_KnockdownSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown", meta=(ClampMin="0.1", Units="s"))
	float DownDuration = 3.0f;

	/** Velocity change applied once on entry, not on subsequent hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown", meta=(ClampMin="0.0"))
	float ImpulseStrength = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown")
	FName PelvisBone = TEXT("pelvis");

	/** Axis pointing out of the front of the pelvis, in that bone's local space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown")
	FVector PelvisFrontAxis = FVector::YAxisVector;

	/** Pelvis-local axis pointing toward the head in the standing skeleton (Mannequin: X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown")
	FVector PelvisUpAxis = FVector::XAxisVector;

	/** Additional facing correction for get-up animations authored with a different forward convention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown", meta=(Units="deg"))
	float GetUpYawOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown")
	TSoftObjectPtr<UAnimMontage> GetUpFaceUpMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown")
	TSoftObjectPtr<UAnimMontage> GetUpFaceDownMontage;

	/** Blend the last physical pose into the start of the get-up montage, once per recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown", meta=(ClampMin="0.0", ClampMax="0.5", Units="s"))
	float GetUpPoseBlendDuration = 0.2f;

	/** Used only when a get-up montage has not been assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Knockdown", meta=(ClampMin="0.1", Units="s"))
	float FallbackGetUpDuration = 0.5f;
};
