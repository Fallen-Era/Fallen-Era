#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/FEChargedProjectileAttackAbility.h"
#include "FEGrenadeAttackAbility.generated.h"

class UAbilityTask_WaitDelay;
class UNiagaraComponent;

/** Charged throwable. Owns only the local trajectory preview. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_GrenadeAttackAbility : public UFE_ChargedProjectileAttackAbility
{
	GENERATED_BODY()

protected:
	virtual void StartSpecializedChargePresentation() override;
	virtual void StopSpecializedChargePresentation() override;
	virtual bool SupportsChargedAttackData(const UFE_ChargedProjectileAttackData* AttackData) const override;

private:
	UFUNCTION()
	void HandleTrajectoryUpdate();

	void UpdateLocalTrajectory();
	void ScheduleTrajectoryUpdate();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> TrajectoryUpdateTask;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TrajectoryComponent;
};
