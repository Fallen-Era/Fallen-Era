#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/PoseSnapshot.h"
#include "Combat/AI/FEAISettings.h"
#include "FEEnemyAnimInstance.generated.h"

class AFE_EnemyCharacter;
class UBlendSpace;

/** AnimBP base that imports the selected character AI preset once it is cached at BeginPlay. */
UCLASS(Blueprintable, BlueprintType)
class FALLENERA_API UFE_EnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Animation")
	FSAISettings GetAISettings() const { return AISettings; }

	static const FName RagdollPoseSnapshotName;
	void BeginRagdollPresentation();
	/** Capture before disabling physics; no bone transforms are replicated. */
	bool CaptureRagdollPose();
	/** Preserve physical child poses after capsule alignment without blending a rotating root. */
	void BeginGetUpPoseBlend(float Duration, float MontageElapsedTime);
	void ClearGetUpPoseBlend();

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Animation")
	float GetRagdollPoseAlpha() const { return RagdollPoseAlpha; }

protected:
	UPROPERTY(BlueprintReadOnly, Transient, Category="FallenEra|AI|Animation")
	FSAISettings AISettings;

	/** Expose the Blend Space asset pin on the AnimGraph player and connect this variable. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="FallenEra|AI|Animation")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace;

	/** Final AnimGraph blend: A = normal full-body output, B = named ragdoll snapshot. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="FallenEra|AI|Animation")
	float RagdollPoseAlpha = 0.0f;

	/** Drives In Ragdoll -> Blend Out Pose in the recovery state machine. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="FallenEra|AI|Animation")
	bool bIsRagdoll = false;

private:
	void TryCacheAISettings();

	TWeakObjectPtr<AFE_EnemyCharacter> EnemyCharacter;
	bool bSettingsCached = false;
	FTransform SnapshotMeshTransform;
	FPoseSnapshot CapturedRagdollPose;
	float PoseBlendStartTime = 0.0f;
	float PoseBlendDuration = 0.0f;
};
