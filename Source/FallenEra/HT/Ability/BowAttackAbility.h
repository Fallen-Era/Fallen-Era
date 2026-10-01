#pragma once

#include "CoreMinimal.h"
#include "HT/Ability/ChargedProjectileAttackAbility.h"
#include "BowAttackAbility.generated.h"

class UAbilityTask_WaitDelay;

/** Charged bow. Owns the camera, reticle, and replicated bow draw alpha. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_BowAttackAbility : public UFE_ChargedProjectileAttackAbility
{
	GENERATED_BODY()

protected:
	virtual void StartSpecializedChargePresentation() override;
	virtual void StopSpecializedChargePresentation() override;
	virtual bool SupportsChargedAttackData(const UFE_ChargedProjectileAttackData* AttackData) const override;

private:
	UFUNCTION()
	void HandleChargePresentationUpdate();

	void UpdateChargePresentation();
	void ScheduleChargePresentationUpdate();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> ChargePresentationUpdateTask;
};
