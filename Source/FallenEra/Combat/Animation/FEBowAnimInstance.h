#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FEBowAnimInstance.generated.h"

/** AnimBP base for a skeletal bow. ChargeAlpha drives a 0..1 draw BlendSpace. */
UCLASS(Blueprintable, BlueprintType)
class FALLENERA_API UFEBowAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="FallenEra|Bow")
	void SetChargeAlpha(float NewChargeAlpha) { ChargeAlpha = FMath::Clamp(NewChargeAlpha, 0.0f, 1.0f); }

	UFUNCTION(BlueprintPure, Category="FallenEra|Bow")
	float GetChargeAlpha() const { return ChargeAlpha; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="FallenEra|Bow")
	float ChargeAlpha = 0.0f;
};
