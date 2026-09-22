#include "HT/Pool/ProjectilePoolSubsystem.h"

#include "Engine/World.h"
#include "HT/Projectile/CombatProjectile.h"
#include "Kismet/GameplayStatics.h"

AFE_CombatProjectile* UFE_ProjectilePoolSubsystem::AcquireProjectile(
	TSubclassOf<AFE_CombatProjectile> ProjectileClass,
	const FTransform& SpawnTransform,
	AActor* Owner,
	APawn* Instigator,
	bool bLocalPreview)
{
	UWorld* World = GetWorld();
	if (!World || !ProjectileClass)
	{
		return nullptr;
	}

	FProjectilePoolBucket& Bucket = Pools.FindOrAdd(ProjectileClass);
	TArray<TWeakObjectPtr<AFE_CombatProjectile>>& InactiveProjectiles = bLocalPreview
		? Bucket.LocalPreviews
		: Bucket.AuthorityProjectiles;

	AFE_CombatProjectile* Projectile = nullptr;
	while (!InactiveProjectiles.IsEmpty() && !Projectile)
	{
		Projectile = InactiveProjectiles.Pop(EAllowShrinking::No).Get();
	}

	if (!Projectile)
	{
		Projectile = World->SpawnActorDeferred<AFE_CombatProjectile>(
			ProjectileClass,
			SpawnTransform,
			Owner,
			Instigator,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile)
		{
			return nullptr;
		}
		UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
	}

	Projectile->ActivateFromPool(SpawnTransform, Owner, Instigator, bLocalPreview);
	return Projectile;
}

bool UFE_ProjectilePoolSubsystem::ReleaseProjectile(AFE_CombatProjectile* Projectile, bool bLocalPreview)
{
	if (!IsValid(Projectile))
	{
		return false;
	}

	const int32 MaxPoolSize = Projectile->GetMaxPoolSize();
	if (MaxPoolSize <= 0)
	{
		return false;
	}

	FProjectilePoolBucket& Bucket = Pools.FindOrAdd(Projectile->GetClass());
	TArray<TWeakObjectPtr<AFE_CombatProjectile>>& InactiveProjectiles = bLocalPreview
		? Bucket.LocalPreviews
		: Bucket.AuthorityProjectiles;
	auto RemoveInvalidEntries = [](TArray<TWeakObjectPtr<AFE_CombatProjectile>>& Entries)
	{
		Entries.RemoveAllSwap(
		[](const TWeakObjectPtr<AFE_CombatProjectile>& Entry)
		{
			return !Entry.IsValid();
		},
		EAllowShrinking::No);
	};
	RemoveInvalidEntries(Bucket.AuthorityProjectiles);
	RemoveInvalidEntries(Bucket.LocalPreviews);

	if (Bucket.AuthorityProjectiles.Num() + Bucket.LocalPreviews.Num() >= MaxPoolSize)
	{
		return false;
	}

	InactiveProjectiles.Add(Projectile);
	return true;
}
