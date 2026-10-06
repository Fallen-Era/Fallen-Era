#include "Combat/Ability/FEBowAttackAbility.h"

#include "GameFramework/Character.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"

bool UFE_BowAttackAbility::SupportsChargedAttackData(
	const UFE_ChargedProjectileAttackData* AttackData) const
{
	return AttackData && AttackData->IsA<UFE_BowAttackData>();
}

void UFE_BowAttackAbility::StartSpecializedChargePresentation()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_BowAttackData* BowData = Cast<UFE_BowAttackData>(CachedChargedAttackData);
	if (!Character || !BowData)
	{
		return;
	}

	if (UFE_CombatComponent* Combat = Character->FindComponentByClass<UFE_CombatComponent>())
	{
		if (Character->IsLocallyControlled())
		{
			Combat->StartChargeCameraPresentation(
				BowData->ChargeCameraRelativeTransform,
				BowData->ChargeCameraBlendInTime,
				BowData->ChargeCameraBlendOutTime);
		}
		Combat->SetBowChargeAlpha(0.0f);
		Combat->SetBowVisualAlpha(0.0f);
	}

}

void UFE_BowAttackAbility::StopSpecializedChargePresentation()
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UFE_CombatComponent* Combat = Character->FindComponentByClass<UFE_CombatComponent>())
		{
			Combat->SetBowChargeAlpha(0.0f);
			Combat->SetBowVisualAlpha(0.0f);
			if (Character->IsLocallyControlled())
			{
				Combat->StopChargeCameraPresentation();
			}
		}
	}
}
