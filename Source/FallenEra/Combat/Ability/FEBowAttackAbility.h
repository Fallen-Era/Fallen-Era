#pragma once

#include "CoreMinimal.h"
#include "Combat/Ability/FEChargedProjectileAttackAbility.h"
#include "FEBowAttackAbility.generated.h"

/** Charged bow. Reticle alpha is computed from the shared charge start time. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_BowAttackAbility : public UFE_ChargedProjectileAttackAbility
{
	GENERATED_BODY()

protected:
	virtual void StartSpecializedChargePresentation() override;
	virtual void StopSpecializedChargePresentation() override;
	virtual bool SupportsChargedAttackData(const UFE_ChargedProjectileAttackData* AttackData) const override;

};
