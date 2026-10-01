#include "Combat/Projectile/CombatProjectile.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Components/CapsuleComponent.h"
#include "Combat/Component/CombatComponent.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/ObjectPool/ProjectilePoolSubsystem.h"
#include "Combat/Weapon/WeaponItemData.h"

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
	CollisionComponent->SetCollisionResponseToChannel(FECollisionChannels::Enemy, ECR_Block);
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
	bIsLocalPreview = false;
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
		// StopSimulating() clears UpdatedComponent after a projectile comes to rest.
		// Pooled instances must restore it before a new velocity can move the actor.
		ProjectileMovement->SetUpdatedComponent(CollisionComponent);
		ProjectileMovement->InitialSpeed = LaunchVelocity.Size();
		// Zero means unlimited. Keeping this equal to the initial speed clamps the
		// gravity-accelerated velocity and makes the real path diverge from the preview.
		ProjectileMovement->MaxSpeed = 0.0f;
		ProjectileMovement->ProjectileGravityScale = GravityScale;
		ProjectileMovement->Velocity = LaunchVelocity;
		ProjectileMovement->Activate(true);
	}
	SetLifeSpan(FMath::Max(0.1f, ProjectileLifeSeconds));
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

void AFE_CombatProjectile::ConfigureAsLocalPreview()
{
	bIsLocalPreview = true;
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

void AFE_CombatProjectile::ActivateFromPool(
	const FTransform& SpawnTransform,
	AActor* NewOwner,
	APawn* NewInstigator,
	bool bInLocalPreview)
{
	bIsInPool = false;
	bIsLocalPreview = bInLocalPreview;
	bHasImpacted = false;
	SetLifeSpan(0.0f);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetOwner(NewOwner);
	SetInstigator(NewInstigator);
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (!bInLocalPreview && HasAuthority())
	{
		SetReplicates(true);
		SetReplicateMovement(true);
		FlushNetDormancy();
		SetNetDormancy(DORM_Awake);
	}
	SetActorHiddenInGame(false);
	SetActorEnableCollision(!bInLocalPreview);
	if (CollisionComponent)
	{
		CollisionComponent->ClearMoveIgnoreActors();
		CollisionComponent->SetCollisionEnabled(
			bInLocalPreview ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
	if (ProjectileMovement)
	{
		ProjectileMovement->SetUpdatedComponent(CollisionComponent);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->Deactivate();
	}
	if (ProjectileMesh)
	{
		ProjectileMesh->SetVisibility(true, true);
	}
	OnActivatedFromPool();
}

void AFE_CombatProjectile::PrepareForPool()
{
	if (bIsInPool)
	{
		return;
	}
	bIsInPool = true;
	SetLifeSpan(0.0f);
	OnReturnedToPool();

	if (CollisionComponent && DamageSource)
	{
		CollisionComponent->IgnoreActorWhenMoving(DamageSource, false);
	}
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->Deactivate();
	}
	SetActorEnableCollision(false);
	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	DamageSource = nullptr;
	WeaponData = nullptr;
	AttackData = nullptr;
	DamageEffectClass = nullptr;
	SetOwner(nullptr);
	SetInstigator(nullptr);

	if (!bIsLocalPreview && HasAuthority())
	{
		ForceNetUpdate();
		SetNetDormancy(DORM_DormantAll);
	}
}

void AFE_CombatProjectile::ReturnToPool()
{
	if (bIsInPool || !GetWorld())
	{
		return;
	}

	const bool bWasLocalPreview = bIsLocalPreview;
	PrepareForPool();
	UFE_ProjectilePoolSubsystem* Pool = GetWorld()->GetSubsystem<UFE_ProjectilePoolSubsystem>();
	if (!Pool || !Pool->ReleaseProjectile(this, bWasLocalPreview))
	{
		Destroy();
	}
}

void AFE_CombatProjectile::LifeSpanExpired()
{
	ReturnToPool();
}

void AFE_CombatProjectile::OnActivatedFromPool()
{
	ReceiveActivatedFromPool(bIsLocalPreview);
}

void AFE_CombatProjectile::OnReturnedToPool()
{
	ReceiveReturnedToPool();
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
	ReturnToPool();
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
