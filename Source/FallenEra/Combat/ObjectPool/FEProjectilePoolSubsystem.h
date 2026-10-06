#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FEProjectilePoolSubsystem.generated.h"

class AFE_CombatProjectile;

USTRUCT()
struct FFE_ProjectilePoolBucket
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AFE_CombatProjectile>> AuthorityProjectiles;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AFE_CombatProjectile>> LocalPreviews;
};

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
	UPROPERTY(Transient)
	TMap<TSubclassOf<AFE_CombatProjectile>, FFE_ProjectilePoolBucket> Pools;
};
