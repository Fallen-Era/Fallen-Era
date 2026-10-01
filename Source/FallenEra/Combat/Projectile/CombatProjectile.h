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
class UFE_ProjectilePoolSubsystem;

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

	/** Deactivates this instance and returns it to the bounded world pool. */
	void ReturnToPool();

	int32 GetMaxPoolSize() const { return MaxPoolSize; }

	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }
	UShapeComponent* GetActiveCollisionComponent() const { return CollisionComponent; }

protected:
	virtual void LifeSpanExpired() override;

	/** Replaces the collision shape used for movement and impact handling. */
	void SetActiveCollisionComponent(UShapeComponent* NewCollisionComponent);

	/** Called after the common authority/source validation for a blocking hit. */
	virtual void ProcessProjectileHit(AActor* OtherActor, const FHitResult& Hit);

	/** Applies this projectile's configured damage GE and reaction to one target. */
	bool ApplyProjectileDamage(AActor* TargetActor, const FHitResult& Hit);
	virtual void OnActivatedFromPool();
	virtual void OnReturnedToPool();

	/** Reset Blueprint-owned trails, audio, or other transient state when an instance is reused. */
	UFUNCTION(BlueprintImplementableEvent, Category="FallenEra|Projectile|Pool", meta=(DisplayName="On Activated From Pool"))
	void ReceiveActivatedFromPool(bool bLocalPreview);

	/** Stop Blueprint-owned effects and clear transient state before this instance becomes inactive. */
	UFUNCTION(BlueprintImplementableEvent, Category="FallenEra|Projectile|Pool", meta=(DisplayName="On Returned To Pool"))
	void ReceiveReturnedToPool();

	AActor* GetDamageSource() const { return DamageSource; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UShapeComponent> CollisionComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Projectile", meta=(ClampMin="0.1"))
	float ProjectileLifeSeconds = 10.0f;

	/** Maximum total inactive authoritative instances and previews retained per class. Zero disables pooling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Projectile|Pool", meta=(ClampMin="0", ClampMax="256"))
	int32 MaxPoolSize = 32;

	UFUNCTION()
	void HandleProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

private:
	friend class UFE_ProjectilePoolSubsystem;

	void ActivateFromPool(
		const FTransform& SpawnTransform,
		AActor* NewOwner,
		APawn* NewInstigator,
		bool bInLocalPreview);
	void PrepareForPool();

	UPROPERTY(Transient)
	TObjectPtr<AActor> DamageSource;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponItemData> WeaponData;

	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponAttackData> AttackData;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	bool bHasImpacted = false;
	bool bIsLocalPreview = false;
	bool bIsInPool = false;
};
