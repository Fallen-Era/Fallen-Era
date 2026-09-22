#pragma once

#include "CoreMinimal.h"
#include "HT/Projectile/CombatProjectile.h"
#include "GrenadeProjectile.generated.h"

class USphereComponent;

/** Timed, bouncing projectile that applies the common damage GE to actors in an explosion sphere. */
UCLASS(Blueprintable)
class FALLENERA_API AFE_GrenadeProjectile : public AFE_CombatProjectile
{
	GENERATED_BODY()

public:
	AFE_GrenadeProjectile();

	virtual void InitializeProjectile(
		AActor* InDamageSource,
		const UFE_WeaponItemData* InWeaponData,
		const UFE_WeaponAttackData* InAttackData,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		const FVector& LaunchVelocity,
		float GravityScale) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ProcessProjectileHit(AActor* OtherActor, const FHitResult& Hit) override;

	/** Collision used while the grenade is flying and bouncing. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Grenade")
	TObjectPtr<USphereComponent> GrenadeCollisionComponent;

	/** Query-only sphere enabled briefly when the fuse expires. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Grenade")
	TObjectPtr<USphereComponent> ExplosionCollisionComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Grenade", meta=(ClampMin="0.0"))
	float FuseTime = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Grenade", meta=(ClampMin="0.0"))
	float ExplosionRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Grenade", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Bounciness = 0.35f;

private:
	void Explode();

	FTimerHandle FuseTimerHandle;
	bool bHasExploded = false;
};
