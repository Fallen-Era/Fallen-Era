#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatProjectile.generated.h"

class UCapsuleComponent;
class UShapeComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;
class UFE_WeaponItemData;
class UFE_WeaponAttackData;

/** Server-authoritative projectile that routes impact damage through CombatComponent. */
UCLASS(Blueprintable)
class FALLENERA_API AFE_CombatProjectile : public AActor
{
	GENERATED_BODY()

public:
	AFE_CombatProjectile();

	/** Called by the spawning ability on the server before the first movement tick. */
	virtual void InitializeProjectile(
		AActor* InDamageSource,
		const UFE_WeaponItemData* InWeaponData,
		const UFE_WeaponAttackData* InAttackData,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		const FVector& LaunchVelocity,
		float GravityScale);

	/** Makes a locally spawned actor safe to use as an attached charge preview. */
	void ConfigureAsLocalPreview();

	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }
	UShapeComponent* GetActiveCollisionComponent() const { return CollisionComponent; }

protected:
	/** Replaces the collision shape used for movement and impact handling. */
	void SetActiveCollisionComponent(UShapeComponent* NewCollisionComponent);

	/** Called after the common authority/source validation for a blocking hit. */
	virtual void ProcessProjectileHit(AActor* OtherActor, const FHitResult& Hit);

	/** Applies this projectile's configured damage GE and reaction to one target. */
	bool ApplyProjectileDamage(AActor* TargetActor, const FHitResult& Hit);

	AActor* GetDamageSource() const { return DamageSource; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UShapeComponent> CollisionComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Projectile", meta=(ClampMin="0.1"))
	float ProjectileLifeSeconds = 10.0f;

	UFUNCTION()
	void HandleProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> DamageSource;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponItemData> WeaponData;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponAttackData> AttackData;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	bool bHasImpacted = false;
};
