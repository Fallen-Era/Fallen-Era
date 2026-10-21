#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;
class UWorld;

/** Box sweep with its front face bounded by reach, and no volume behind the camera. */
struct FFE_CameraMeleeTraceVolume
{
	FVector ViewStart = FVector::ZeroVector;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FVector HalfExtent = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	bool bValid = false;
};

namespace FECameraMeleeTrace
{
	FALLENERA_API FFE_CameraMeleeTraceVolume MakeVolume(const FVector& ViewStart,
		const FRotator& ViewRotation, float Range, const FVector& HalfExtent);

	/** Center hit comes first. The full-range area sweep overlaps enemies and stops at world blockers. */
	FALLENERA_API void Sweep(UWorld& World, const AActor* Source,
		const FFE_CameraMeleeTraceVolume& Volume, ECollisionChannel CenterChannel, TArray<FHitResult>& OutHits,
		FHitResult* OutCenterHit = nullptr);

	/** Area participants opt in through collision, without depending on a concrete character class. */
	FALLENERA_API bool IsAreaTarget(const FHitResult& Hit);
}
