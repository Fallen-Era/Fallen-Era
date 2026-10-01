#pragma once

#include "CoreMinimal.h"
#include "Combat/Projectile/CombatProjectile.h"
#include "ArrowProjectile.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UGameplayEffect;
class UFE_WeaponItemData;
class UFE_WeaponAttackData;

/** Arrow projectile using a narrow box collision aligned with its flight direction. */
UCLASS(Blueprintable)
class FALLENERA_API AFE_ArrowProjectile : public AFE_CombatProjectile
{
	GENERATED_BODY()

public:
	AFE_ArrowProjectile();

	virtual void InitializeProjectile(
		AActor* InDamageSource,
		const UFE_WeaponItemData* InWeaponData,
		const UFE_WeaponAttackData* InAttackData,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		const FVector& LaunchVelocity,
		float GravityScale) override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void OnActivatedFromPool() override;
	virtual void OnReturnedToPool() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Projectile")
	TObjectPtr<UBoxComponent> ArrowCollisionComponent;

	/** Assign the arrow trail Niagara System on the derived Blueprint component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Projectile|Trail")
	TObjectPtr<UNiagaraComponent> ArrowTrailComponent;

private:
	UFUNCTION()
	void OnRep_TrailActive();

	void SetTrailActive(bool bNewActive);
	void ApplyTrailState();

	/** Replicated explicitly because Niagara activation is not propagated by movement replication. */
	UPROPERTY(ReplicatedUsing=OnRep_TrailActive)
	bool bTrailActive = false;
};
