#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "FEAnimNotifyState_BowChargeCurve.generated.h"

/**
 * Samples a 0..1 float curve from the character charge animation and applies it
 * locally to both equipped bow meshes. Add this notify state across the charge
 * section of the character animation that owns the curve.
 */
UCLASS(Blueprintable, meta=(DisplayName="FE Bow Charge Curve"))
class FALLENERA_API UFE_AnimNotifyState_BowChargeCurve : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FallenEra|Bow")
	FName CurveName = TEXT("BowDrawAlpha");

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	void ApplyCurveValue(USkeletalMeshComponent* MeshComp) const;
	void ApplyVisualAlpha(USkeletalMeshComponent* MeshComp, float VisualAlpha) const;
};
