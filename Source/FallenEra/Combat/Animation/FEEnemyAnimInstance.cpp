#include "Combat/Animation/FEEnemyAnimInstance.h"

#include "Combat/Character/FEEnemyCharacter.h"
#include "Animation/PoseSnapshot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"

const FName UFE_EnemyAnimInstance::RagdollPoseSnapshotName(TEXT("FE_RagdollPose"));

void UFE_EnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	ClearGetUpPoseBlend();
	EnemyCharacter = Cast<AFE_EnemyCharacter>(TryGetPawnOwner());
	bSettingsCached = false;
	TryCacheAISettings();
}

void UFE_EnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!bIsRagdoll && RagdollPoseAlpha > 0.0f && PoseBlendDuration > 0.0f && GetWorld())
	{
		const float Progress = FMath::Clamp((GetWorld()->GetTimeSeconds() - PoseBlendStartTime) / PoseBlendDuration, 0.0f, 1.0f);
		RagdollPoseAlpha = 1.0f - FMath::SmoothStep(0.0f, 1.0f, Progress);
	}
	if (!bSettingsCached)
	{
		if (!EnemyCharacter.IsValid())
		{
			EnemyCharacter = Cast<AFE_EnemyCharacter>(TryGetPawnOwner());
		}
		TryCacheAISettings();
	}
}

void UFE_EnemyAnimInstance::BeginRagdollPresentation()
{
	ClearGetUpPoseBlend();
	bIsRagdoll = true;
}

bool UFE_EnemyAnimInstance::CaptureRagdollPose()
{
	ClearGetUpPoseBlend();
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset() || Mesh->GetComponentSpaceTransforms().IsEmpty() ||
		!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) { return false; }
	SnapshotPose(CapturedRagdollPose);
	SnapshotMeshTransform = Mesh->GetComponentTransform();
	return CapturedRagdollPose.bIsValid && !CapturedRagdollPose.LocalTransforms.IsEmpty();
}

void UFE_EnemyAnimInstance::BeginGetUpPoseBlend(float Duration, float MontageElapsedTime)
{
	if (!CapturedRagdollPose.bIsValid || CapturedRagdollPose.LocalTransforms.IsEmpty() ||
		!GetWorld() || !GetSkelMeshComponent() || Duration <= KINDA_SMALL_NUMBER || MontageElapsedTime >= Duration)
	{
		ClearGetUpPoseBlend();
		return;
	}
	// Both poses must share a stationary root, as in ALS. Blending a fallen root rotation
	// against an upright root makes the pelvis describe an upward arc during the snapshot blend.
	// Bake any final capsule alignment into root children once, preserving every physical child world pose.
	const FReferenceSkeleton& Skeleton = GetSkelMeshComponent()->GetSkeletalMeshAsset()->GetRefSkeleton();
	const FTransform PreviousRoot = CapturedRagdollPose.LocalTransforms[0];
	const FTransform StableRoot = Skeleton.GetRefBonePose()[0];
	const FTransform NewMeshTransform = GetSkelMeshComponent()->GetComponentTransform();
	for (int32 Index = 1; Index < CapturedRagdollPose.LocalTransforms.Num(); ++Index)
	{
		const int32 BoneIndex = Skeleton.FindBoneIndex(CapturedRagdollPose.BoneNames[Index]);
		if (BoneIndex != INDEX_NONE && Skeleton.GetParentIndex(BoneIndex) == 0)
		{
			const FTransform ChildWorld = CapturedRagdollPose.LocalTransforms[Index] * PreviousRoot * SnapshotMeshTransform;
			CapturedRagdollPose.LocalTransforms[Index] = ChildWorld.GetRelativeTransform(NewMeshTransform).GetRelativeTransform(StableRoot);
		}
	}
	CapturedRagdollPose.LocalTransforms[0] = StableRoot;
	CapturedRagdollPose.SnapshotName = RagdollPoseSnapshotName;
	// AddPoseSnapshot resets an existing entry; move ownership only after preparing the complete pose.
	AddPoseSnapshot(RagdollPoseSnapshotName) = MoveTemp(CapturedRagdollPose);
	CapturedRagdollPose.Reset();
	PoseBlendDuration = Duration;
	PoseBlendStartTime = GetWorld()->GetTimeSeconds() - FMath::Max(0.0f, MontageElapsedTime);
	const float Progress = FMath::Clamp(MontageElapsedTime / Duration, 0.0f, 1.0f);
	RagdollPoseAlpha = 1.0f - FMath::SmoothStep(0.0f, 1.0f, Progress);
}

void UFE_EnemyAnimInstance::ClearGetUpPoseBlend()
{
	RagdollPoseAlpha = 0.0f;
	bIsRagdoll = false;
	PoseBlendDuration = 0.0f;
	CapturedRagdollPose.Reset();
	RemovePoseSnapshot(RagdollPoseSnapshotName);
}

void UFE_EnemyAnimInstance::TryCacheAISettings()
{
	if (!EnemyCharacter.IsValid() || !EnemyCharacter->IsAISettingsCached())
	{
		return;
	}

	AISettings = EnemyCharacter->GetCachedAISettings();
	LocomotionBlendSpace = AISettings.LocomotionBlendSpace;
	bSettingsCached = true;
}
