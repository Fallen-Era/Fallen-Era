#include "HT/Projectile/ArrowProjectile.h"

#include "Components/BoxComponent.h"

AFE_ArrowProjectile::AFE_ArrowProjectile()
{
	ArrowCollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("ArrowCollisionComponent"));
	ArrowCollisionComponent->SetupAttachment(RootComponent);
	ArrowCollisionComponent->SetBoxExtent(FVector(40.0f, 3.0f, 3.0f));

	SetActiveCollisionComponent(ArrowCollisionComponent);
}
