#pragma once

#include "CoreMinimal.h"
#include "HT/Projectile/CombatProjectile.h"
#include "ArrowProjectile.generated.h"

class UBoxComponent;

/** Arrow projectile using a narrow box collision aligned with its flight direction. */
UCLASS(Blueprintable)
class FALLENERA_API AFE_ArrowProjectile : public AFE_CombatProjectile
{
	GENERATED_BODY()

public:
	AFE_ArrowProjectile();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Projectile")
	TObjectPtr<UBoxComponent> ArrowCollisionComponent;
};
