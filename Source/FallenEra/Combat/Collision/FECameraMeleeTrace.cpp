#include "Combat/Collision/FECameraMeleeTrace.h"
#include "Combat/Collision/FECollisionChannels.h"

#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"

FFE_CameraMeleeTraceVolume FECameraMeleeTrace::MakeVolume(const FVector& ViewStart,
	const FRotator& ViewRotation, float Range, const FVector& HalfExtent)
{
	FFE_CameraMeleeTraceVolume Volume;
	if (!FMath::IsFinite(Range) || Range <= KINDA_SMALL_NUMBER ||
		ViewStart.ContainsNaN() || ViewRotation.ContainsNaN() || HalfExtent.ContainsNaN())
	{
		return Volume;
	}
	Volume.ViewStart = ViewStart;
	Volume.Rotation = ViewRotation.Quaternion();
	Volume.HalfExtent = HalfExtent.ComponentMax(FVector(0.1f));
	Volume.HalfExtent.X = FMath::Min(Volume.HalfExtent.X, Range * 0.5f);
	const FVector Forward = ViewRotation.Vector();
	Volume.Start = ViewStart + Forward * Volume.HalfExtent.X;
	Volume.End = ViewStart + Forward * (Range - Volume.HalfExtent.X);
	Volume.bValid = true;
	return Volume;
}

void FECameraMeleeTrace::Sweep(UWorld& World, const AActor* Source,
	const FFE_CameraMeleeTraceVolume& Volume, ECollisionChannel CenterChannel, TArray<FHitResult>& OutHits,
	FHitResult* OutCenterHit)
{
	OutHits.Reset();
	if (OutCenterHit) { *OutCenterHit = FHitResult(); }
	if (!Volume.bValid) { return; }
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FE_CameraMeleeAttack), false, Source);
	QueryParams.bReturnPhysicalMaterial = true;
	const FVector Forward = Volume.Rotation.GetForwardVector();
	const FVector ReachEnd = Volume.End + Forward * Volume.HalfExtent.X;
	FHitResult CenterHit;
	const bool bCenterHit = World.LineTraceSingleByChannel(
		CenterHit, Volume.ViewStart, ReachEnd, CenterChannel, QueryParams);
	TSet<const AActor*> HitActors;
	if (bCenterHit)
	{
		OutHits.Add(CenterHit);
		HitActors.Add(CenterHit.GetActor());
		if (OutCenterHit) { *OutCenterHit = CenterHit; }
	}
	if (Volume.bValid)
	{
		TArray<FHitResult> AreaHits;
		World.SweepMultiByChannel(AreaHits, Volume.Start, Volume.End, Volume.Rotation,
			FECollisionChannels::PlayerMeleeAreaTrace, FCollisionShape::MakeBox(Volume.HalfExtent), QueryParams);
		// A camera-adjacent box already inside a wall has no safe sweep interval.
		// Keep the independently verified center hit, but fail closed for area damage.
		const bool bStartsBlocked = AreaHits.ContainsByPredicate(
			[](const FHitResult& Hit) { return Hit.bBlockingHit && Hit.bStartPenetrating; });
		if (!bStartsBlocked)
		{
			for (const FHitResult& Hit : AreaHits)
			{
				const AActor* Target = Hit.GetActor();
				if (Target && IsAreaTarget(Hit) && !HitActors.Contains(Target))
				{
					HitActors.Add(Target);
					OutHits.Add(Hit);
				}
			}
		}
	}
	for (FHitResult& Hit : OutHits)
	{
		const UPrimitiveComponent* Component = Hit.GetComponent();
		// Newly created/query-only bodies may have no material in Chaos' query cache yet.
		// This is a simple-body query: resolve the actual hit body, including skeletal bone overrides.
		if (!Hit.PhysMaterial.IsValid() && Component)
		{
			if (const FBodyInstance* Body = Component->GetBodyInstance(Hit.BoneName))
			{
				Hit.PhysMaterial = Body->GetSimplePhysicalMaterial();
			}
		}
		if (Hit.bStartPenetrating)
		{
			FVector Contact = Hit.Location;
			if (Component)
			{
				FVector ClosestPoint;
				if (Component->GetClosestPointOnCollision(Volume.ViewStart, ClosestPoint, Hit.BoneName) >= 0.0f)
				{
					Contact = ClosestPoint;
				}
			}
			Hit.ImpactPoint = Contact;
		}
	}
}

bool FECameraMeleeTrace::IsAreaTarget(const FHitResult& Hit)
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	return Component && Component->GetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeAreaTrace) == ECR_Overlap;
}
