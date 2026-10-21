#include "Combat/Ability/FEWeaponAimAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Weapon/FEWeaponItemData.h"

UFE_WeaponAimAbility::UFE_WeaponAimAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = EFallenEraAbilityActivationPolicy::WhileInputActive;
	SetAssetTags(FGameplayTagContainer(FallenEraCombatGameplayTags::Ability_Combat_Aim));
	ActivationOwnedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_Aiming);
	ActivationBlockedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_Reloading);
	ActivationBlockedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_SelectingAmmo);
}

bool UFE_WeaponAimAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) ||
		!ActorInfo || !ActorInfo->AvatarActor.IsValid()) { return false; }
	const AActor* Actor = ActorInfo->AvatarActor.Get();
	const UFE_EquipmentComponent* Equipment = Actor->FindComponentByClass<UFE_EquipmentComponent>();
	const UFE_RangedWeaponItemData* Weapon = Equipment ? Cast<UFE_RangedWeaponItemData>(Equipment->GetCurrentWeaponData()) : nullptr;
	const UFE_WeaponAimingComponent* Component = Actor->FindComponentByClass<UFE_WeaponAimingComponent>();
	return Weapon && Weapon->Template.bSupportsAiming && GetSourceObject(Handle, ActorInfo) == Weapon && Component && Component->CanAim();
}

void UFE_WeaponAimAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Aiming = ActorInfo->AvatarActor->FindComponentByClass<UFE_WeaponAimingComponent>();
	if (!Aiming.IsValid() || !Aiming->BeginAim())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Held-policy retry can reactivate after a release/re-press with Spec.InputPressed
	// still false. Wait for the new activation's release event, not that stale flag.
	UAbilityTask_WaitInputRelease* Task = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	Task->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
	Task->ReadyForActivation();
}

void UFE_WeaponAimAbility::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFE_WeaponAimAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (Aiming.IsValid()) { Aiming->EndAim(bWasCancelled); }
	Aiming.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
