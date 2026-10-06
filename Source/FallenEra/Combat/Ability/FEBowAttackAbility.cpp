#include "Combat/Ability/FEBowAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
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

	if (Character->IsLocallyControlled())
	{
		UpdateChargePresentation();
		ScheduleChargePresentationUpdate();
	}
}

void UFE_BowAttackAbility::StopSpecializedChargePresentation()
{
	if (ChargePresentationUpdateTask)
	{
		ChargePresentationUpdateTask->EndTask();
		ChargePresentationUpdateTask = nullptr;
	}
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

void UFE_BowAttackAbility::HandleChargePresentationUpdate()
{
	ChargePresentationUpdateTask = nullptr;
	if (!bReleaseRequested && IsActive())
	{
		UpdateChargePresentation();
		ScheduleChargePresentationUpdate();
	}
}

void UFE_BowAttackAbility::UpdateChargePresentation()
{
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UFE_CombatComponent* Combat = Character->FindComponentByClass<UFE_CombatComponent>())
		{
			Combat->SetBowChargeAlpha(CalculateChargeAlpha());
		}
	}
}

void UFE_BowAttackAbility::ScheduleChargePresentationUpdate()
{
	if (bReleaseRequested || !IsActive())
	{
		return;
	}
	ChargePresentationUpdateTask = UAbilityTask_WaitDelay::WaitDelay(this, 1.0f / 60.0f);
	ChargePresentationUpdateTask->OnFinish.AddDynamic(
		this, &UFE_BowAttackAbility::HandleChargePresentationUpdate);
	ChargePresentationUpdateTask->ReadyForActivation();
}
