#include "HT/Projectile/GrenadeProjectile.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"

AFE_GrenadeProjectile::AFE_GrenadeProjectile()
{
	GrenadeCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("GrenadeCollisionComponent"));
	GrenadeCollisionComponent->InitSphereRadius(8.0f);
	GrenadeCollisionComponent->SetupAttachment(RootComponent);
	SetActiveCollisionComponent(GrenadeCollisionComponent);

	ExplosionCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("ExplosionCollisionComponent"));
	ExplosionCollisionComponent->SetupAttachment(RootComponent);
	ExplosionCollisionComponent->SetSphereRadius(ExplosionRadius);
	ExplosionCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExplosionCollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	ExplosionCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExplosionCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ExplosionCollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ExplosionCollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	ExplosionCollisionComponent->SetGenerateOverlapEvents(true);

	if (ProjectileMovement)
	{
		ProjectileMovement->bRotationFollowsVelocity = false;
		ProjectileMovement->bShouldBounce = true;
		ProjectileMovement->Bounciness = Bounciness;
		ProjectileMovement->Friction = 0.5f;
	}
}

void AFE_GrenadeProjectile::InitializeProjectile(
	AActor* InDamageSource,
	const UFE_WeaponItemData* InWeaponData,
	const UFE_WeaponAttackData* InAttackData,
	TSubclassOf<UGameplayEffect> InDamageEffectClass,
	const FVector& LaunchVelocity,
	float GravityScale)
{
	Super::InitializeProjectile(
		InDamageSource,
		InWeaponData,
		InAttackData,
		InDamageEffectClass,
		LaunchVelocity,
		GravityScale);

	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->Bounciness = Bounciness;
	}
	SetLifeSpan(FMath::Max(ProjectileLifeSeconds, FuseTime + 1.0f));
	GetWorldTimerManager().SetTimer(
		FuseTimerHandle,
		this,
		&AFE_GrenadeProjectile::Explode,
		FMath::Max(0.01f, FuseTime),
		false);
}

void AFE_GrenadeProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FuseTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AFE_GrenadeProjectile::ProcessProjectileHit(AActor* OtherActor, const FHitResult& Hit)
{
	// Contact only changes the projectile's movement through bounce handling.
	// Damage is exclusively applied by Explode after the fuse expires.
	(void)OtherActor;
	(void)Hit;
}

void AFE_GrenadeProjectile::Explode()
{
	if (!HasAuthority() || bHasExploded || !ExplosionCollisionComponent)
	{
		return;
	}

	bHasExploded = true;
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	ExplosionCollisionComponent->SetSphereRadius(FMath::Max(0.0f, ExplosionRadius), true);
	ExplosionCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExplosionCollisionComponent->UpdateOverlaps();

	TSet<AActor*> OverlappingActors;
	ExplosionCollisionComponent->GetOverlappingActors(OverlappingActors);
	ExplosionCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const FVector ExplosionLocation = GetActorLocation();
	for (AActor* TargetActor : OverlappingActors)
	{
		if (!IsValid(TargetActor) || TargetActor == this || TargetActor == GetDamageSource())
		{
			continue;
		}

		const FVector TargetLocation = TargetActor->GetActorLocation();
		const FVector ImpactNormal = (TargetLocation - ExplosionLocation).GetSafeNormal(
			UE_SMALL_NUMBER,
			FVector::UpVector);
		FHitResult ExplosionHit;
		ExplosionHit.Location = TargetLocation;
		ExplosionHit.ImpactPoint = TargetLocation;
		ExplosionHit.Normal = ImpactNormal;
		ExplosionHit.ImpactNormal = ImpactNormal;
		ApplyProjectileDamage(TargetActor, ExplosionHit);
	}

	Destroy();
}
