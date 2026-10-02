#include "Combat/Projectile/FEArrowProjectile.h"

#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"

AFE_ArrowProjectile::AFE_ArrowProjectile()
{
	ArrowCollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("ArrowCollisionComponent"));
	ArrowCollisionComponent->SetupAttachment(RootComponent);
	ArrowCollisionComponent->SetBoxExtent(FVector(40.0f, 3.0f, 3.0f));

	SetActiveCollisionComponent(ArrowCollisionComponent);

	ArrowTrailComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ArrowTrailComponent"));
	ArrowTrailComponent->SetupAttachment(RootComponent);
	ArrowTrailComponent->SetAutoActivate(false);
	ArrowTrailComponent->SetActive(false);
}

void AFE_ArrowProjectile::InitializeProjectile(
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

	SetTrailActive(true);
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

void AFE_ArrowProjectile::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFE_ArrowProjectile, bTrailActive);
}

void AFE_ArrowProjectile::OnActivatedFromPool()
{
	SetTrailActive(false);
	Super::OnActivatedFromPool();
}

void AFE_ArrowProjectile::OnReturnedToPool()
{
	SetTrailActive(false);
	Super::OnReturnedToPool();
}

void AFE_ArrowProjectile::OnRep_TrailActive()
{
	ApplyTrailState();
}

void AFE_ArrowProjectile::SetTrailActive(bool bNewActive)
{
	bTrailActive = bNewActive;
	ApplyTrailState();
}

void AFE_ArrowProjectile::ApplyTrailState()
{
	if (!ArrowTrailComponent)
	{
		return;
	}

	if (bTrailActive)
	{
		ArrowTrailComponent->Activate(true);
	}
	else
	{
		ArrowTrailComponent->DeactivateImmediate();
	}
}
