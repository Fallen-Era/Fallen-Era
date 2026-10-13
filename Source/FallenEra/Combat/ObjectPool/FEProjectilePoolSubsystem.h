#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FEProjectilePoolSubsystem.generated.h"

class AFE_CombatProjectile;

/** Bounded per-world pool for authoritative projectiles and local charge previews. */
UCLASS()
class FALLENERA_API UFE_ProjectilePoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	AFE_CombatProjectile* AcquireProjectile(
		TSubclassOf<AFE_CombatProjectile> ProjectileClass,
		const FTransform& SpawnTransform,
		AActor* Owner,
		APawn* Instigator,
		bool bLocalPreview);

	/** Returns false when the bounded pool is full and the caller should destroy the actor. */
	bool ReleaseProjectile(AFE_CombatProjectile* Projectile, bool bLocalPreview);

private:
	struct FProjectilePoolBucket
	{
		TArray<TWeakObjectPtr<AFE_CombatProjectile>> AuthorityProjectiles;
		TArray<TWeakObjectPtr<AFE_CombatProjectile>> LocalPreviews;
	};

	TMap<TSubclassOf<AFE_CombatProjectile>, FProjectilePoolBucket> Pools;
};
