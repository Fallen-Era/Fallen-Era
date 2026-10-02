#include "Combat/Spawn/FEMonsterSpawnRegion.h"

#include "Components/BoxComponent.h"

AFE_MonsterSpawnRegion::AFE_MonsterSpawnRegion()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SpawnBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBounds"));
	SetRootComponent(SpawnBounds);
	SpawnBounds->SetBoxExtent(FVector(10000.0f, 10000.0f, 3000.0f));
	SpawnBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool AFE_MonsterSpawnRegion::ContainsLocation(const FVector& WorldLocation) const
{
	if (!SpawnBounds)
	{
		return false;
	}
	const FVector LocalLocation = SpawnBounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = SpawnBounds->GetUnscaledBoxExtent();
	return FMath::Abs(LocalLocation.X) <= Extent.X &&
		FMath::Abs(LocalLocation.Y) <= Extent.Y &&
		FMath::Abs(LocalLocation.Z) <= Extent.Z;
}
