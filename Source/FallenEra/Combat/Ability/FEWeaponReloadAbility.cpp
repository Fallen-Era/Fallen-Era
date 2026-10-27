#include "Combat/Ability/FEWeaponReloadAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Animation/AnimMontage.h"

UFE_WeaponReloadAbility::UFE_WeaponReloadAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationPolicy = EFallenEraAbilityActivationPolicy::OnInputTriggered;
	SetAssetTags(FGameplayTagContainer(FallenEraCombatGameplayTags::Ability_Combat_Reload));
	ActivationBlockedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown);
	BlockAbilitiesWithTag.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Attack);
	BlockAbilitiesWithTag.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Aim);
}

void UFE_WeaponReloadAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bStartedReload = false;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	Equipment = Avatar ? Avatar->FindComponentByClass<UFE_EquipmentComponent>() : nullptr;
	if (!Equipment.IsValid() || !Equipment->BeginWeaponReload())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bStartedReload = true;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading, 1, EGameplayTagReplicationState::TagAndCountToAll);
	FGameplayTagContainer Actions(FallenEraCombatGameplayTags::Ability_Combat_Attack);
	Actions.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Aim);
	ASC->CancelAbilities(&Actions);
	// Also clear a released aim ability's remaining local blend-out.
	if (UFE_WeaponAimingComponent* Aiming = Avatar->FindComponentByClass<UFE_WeaponAimingComponent>()) { Aiming->EndAim(); }
	const FFE_WeaponAmmoSettings& Settings = Equipment->GetCurrentWeaponData()->Template.AmmoSettings;
	const bool bCharged = Equipment->GetCurrentWeaponData()->Template.AttackActions.ContainsByPredicate(
		[](const TObjectPtr<UFE_WeaponAttackData>& Attack) { return Attack && Attack->IsA<UFE_ChargedProjectileAttackData>(); });
	if (bCharged) { CompleteReload(); return; }
	const auto HasValidPlayRate = [](const UAnimMontage* Montage)
	{
		return !Montage || (FMath::IsFinite(Montage->RateScale) && Montage->RateScale > 0.0f);
	};
	// An assigned but paused/reversed montage is not an animation-free instant reload.
	if (!HasValidPlayRate(Settings.ReloadMontage.Get()) || !HasValidPlayRate(Settings.WeaponReloadMontage.Get()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const auto GetMontageDuration = [](const UAnimMontage* Montage)
	{
		return Montage ? Montage->GetPlayLength() / Montage->RateScale : 0.0f;
	};
	// Character and weapon animations may differ; keep reload/ADS locked until both finish.
	const float MontageTime = FMath::Max(GetMontageDuration(Settings.ReloadMontage.Get()),
		GetMontageDuration(Settings.WeaponReloadMontage.Get()));
	if (MontageTime <= 0.0f) { CompleteReload(); return; }
	if (UFE_CombatComponent* Combat = Avatar->FindComponentByClass<UFE_CombatComponent>())
	{
		Combat->StartReloadPresentation(Settings.ReloadMontage.Get(), Settings.WeaponReloadMontage.Get(), MontageTime);
	}
	ReloadTask = UAbilityTask_WaitDelay::WaitDelay(this, MontageTime);
	ReloadTask->OnFinish.AddDynamic(this, &ThisClass::CompleteReload);
	ReloadTask->ReadyForActivation();
}

void UFE_WeaponReloadAbility::CompleteReload()
{
	const bool bCompleted = Equipment.IsValid() && UFE_CombatComponent::CanActorAttack(GetAvatarActorFromActorInfo()) &&
		Equipment->FinishWeaponReload();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bCompleted);
}

void UFE_WeaponReloadAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ReloadTask) { ReloadTask->EndTask(); ReloadTask = nullptr; }
	if (bStartedReload)
	{
		if (AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			if (UFE_CombatComponent* Combat = Avatar->FindComponentByClass<UFE_CombatComponent>()) { Combat->EndReloadPresentation(); }
		}
		if (Equipment.IsValid()) { Equipment->CancelWeaponReload(); }
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_Reloading, 1, EGameplayTagReplicationState::TagAndCountToAll);
		}
		if (bWasCancelled)
		{
			if (AActor* Avatar = GetAvatarActorFromActorInfo())
			{
				if (UFE_CombatComponent* Combat = Avatar->FindComponentByClass<UFE_CombatComponent>()) { Combat->StopWeaponActionPresentation(false); }
			}
		}
	}
	bStartedReload = false;
	Equipment.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
