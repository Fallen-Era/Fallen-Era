#include "HT/Projectile/CombatProjectile.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Components/CapsuleComponent.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Weapon/WeaponItemData.h"

AFE_CombatProjectile::AFE_CombatProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	UCapsuleComponent* DefaultCollisionComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionComponent"));
	DefaultCollisionComponent->InitCapsuleSize(5.0f, 10.0f);
	SetRootComponent(DefaultCollisionComponent);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(DefaultCollisionComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = DefaultCollisionComponent;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bForceSubStepping = true;
	ProjectileMovement->SetIsReplicated(true);

	SetActiveCollisionComponent(DefaultCollisionComponent);
}

void AFE_CombatProjectile::SetActiveCollisionComponent(UShapeComponent* NewCollisionComponent)
{
	if (!NewCollisionComponent)
	{
		return;
	}

	if (CollisionComponent && CollisionComponent != NewCollisionComponent)
	{
		CollisionComponent->OnComponentHit.RemoveDynamic(this, &AFE_CombatProjectile::HandleProjectileHit);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (RootComponent != NewCollisionComponent)
	{
		USceneComponent* PreviousRoot = RootComponent;
		NewCollisionComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		SetRootComponent(NewCollisionComponent);
		if (PreviousRoot)
		{
			PreviousRoot->SetupAttachment(NewCollisionComponent);
		}
	}

	CollisionComponent = NewCollisionComponent;
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	CollisionComponent->bReturnMaterialOnMove = true;
	CollisionComponent->OnComponentHit.AddUniqueDynamic(this, &AFE_CombatProjectile::HandleProjectileHit);

	if (ProjectileMesh)
	{
		ProjectileMesh->SetupAttachment(CollisionComponent);
	}
	if (ProjectileMovement)
	{
		ProjectileMovement->UpdatedComponent = CollisionComponent;
	}
}

void AFE_CombatProjectile::InitializeProjectile(
	AActor* InDamageSource,
	const UFE_WeaponItemData* InWeaponData,
	const UFE_WeaponAttackData* InAttackData,
	TSubclassOf<UGameplayEffect> InDamageEffectClass,
	const FVector& LaunchVelocity,
	float GravityScale)
{
	DamageSource = InDamageSource;
	WeaponData = const_cast<UFE_WeaponItemData*>(InWeaponData);
	AttackData = const_cast<UFE_WeaponAttackData*>(InAttackData);
	DamageEffectClass = InDamageEffectClass;

	if (CollisionComponent && InDamageSource)
	{
		CollisionComponent->IgnoreActorWhenMoving(InDamageSource, true);
	}
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = LaunchVelocity.Size();
		// Zero means unlimited. Keeping this equal to the initial speed clamps the
		// gravity-accelerated velocity and makes the real path diverge from the preview.
		ProjectileMovement->MaxSpeed = 0.0f;
		ProjectileMovement->ProjectileGravityScale = GravityScale;
		ProjectileMovement->Velocity = LaunchVelocity;
		ProjectileMovement->Activate(true);
	}
	SetLifeSpan(FMath::Max(0.1f, ProjectileLifeSeconds));
}

void AFE_CombatProjectile::ConfigureAsLocalPreview()
{
	SetReplicates(false);
	SetReplicateMovement(false);
	SetActorEnableCollision(false);
	SetLifeSpan(0.0f);
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
}

void AFE_CombatProjectile::HandleProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	(void)HitComponent;
	(void)OtherComponent;
	(void)NormalImpulse;

	if (!HasAuthority() || bHasImpacted || !DamageSource || OtherActor == DamageSource)
	{
		return;
	}

	ProcessProjectileHit(OtherActor, Hit);
}

void AFE_CombatProjectile::ProcessProjectileHit(AActor* OtherActor, const FHitResult& Hit)
{
	if (bHasImpacted)
	{
		return;
	}

	bHasImpacted = true;
	ApplyProjectileDamage(OtherActor, Hit);
	Destroy();
}

bool AFE_CombatProjectile::ApplyProjectileDamage(AActor* TargetActor, const FHitResult& Hit)
{
	if (!HasAuthority() || !DamageSource || !TargetActor || TargetActor == DamageSource)
	{
		return false;
	}

	if (UFE_CombatComponent* SourceCombat = DamageSource->FindComponentByClass<UFE_CombatComponent>())
	{
		return DamageEffectClass
			? SourceCombat->ApplyDamageWithEffectFromHit(TargetActor, DamageEffectClass, Hit, AttackData)
			: SourceCombat->ApplyDamageFromHit(TargetActor, Hit, AttackData);
	}

	return false;
}
