#include "Combat/Animation/FEAnimNotifyState_BowChargeCurve.h"

#include "Animation/AnimInstance.h"
#include "Combat/Component/FECombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UFE_AnimNotifyState_BowChargeCurve::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	(void)Animation;
	(void)TotalDuration;
	(void)EventReference;
	ApplyCurveValue(MeshComp);
}

void UFE_AnimNotifyState_BowChargeCurve::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	(void)Animation;
	(void)FrameDeltaTime;
	(void)EventReference;
	ApplyCurveValue(MeshComp);
}

void UFE_AnimNotifyState_BowChargeCurve::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	(void)Animation;
	(void)EventReference;
	// Finishing the pull animation does not release the attack. Keep the bow fully
	// drawn until the ability receives input release, cancellation, or a weapon swap.
	ApplyVisualAlpha(MeshComp, 1.0f);
}

FString UFE_AnimNotifyState_BowChargeCurve::GetNotifyName_Implementation() const
{
	return CurveName.IsNone()
		? TEXT("FE Bow Charge Curve")
		: FString::Printf(TEXT("FE Bow Charge Curve: %s"), *CurveName.ToString());
}

void UFE_AnimNotifyState_BowChargeCurve::ApplyCurveValue(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp || CurveName.IsNone())
	{
		return;
	}

	const UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (AnimInstance)
	{
		ApplyVisualAlpha(MeshComp, AnimInstance->GetCurveValue(CurveName));
	}
}

void UFE_AnimNotifyState_BowChargeCurve::ApplyVisualAlpha(
	USkeletalMeshComponent* MeshComp,
	float VisualAlpha) const
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UFE_CombatComponent* Combat = OwnerActor
		? OwnerActor->FindComponentByClass<UFE_CombatComponent>()
		: nullptr)
	{
		Combat->SetBowVisualAlpha(VisualAlpha);
	}
}
