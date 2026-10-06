#include "Combat/Ability/FEChargedProjectileAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Projectile/FECombatProjectile.h"
#include "Combat/Weapon/FEWeaponItemData.h"

UFE_ChargedProjectileAttackAbility::UFE_ChargedProjectileAttackAbility()
{
	// A held re-press keeps retrying activation until FireInterval has elapsed.
	ActivationPolicy = EFallenEraAbilityActivationPolicy::WhileInputActive;
}

void UFE_ChargedProjectileAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UFallenEraGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bReleaseRequested = false;
	bProjectileFired = false;
	bChargePresentationActive = false;
	ReleasedChargeAlpha = 0.0f;
	CachedChargedAttackData = nullptr;

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? GetWeaponData(CombatCharacter) : nullptr;
	const UFE_ChargedProjectileAttackData* AttackData = Cast<UFE_ChargedProjectileAttackData>(ResolveAttackData(CombatCharacter));
	if (!CombatCharacter || !WeaponData || !AttackData || !SupportsChargedAttackData(AttackData) ||
		!CanFireProjectile(CombatCharacter, AttackData) ||
		!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CacheAttackContext(WeaponData, AttackData);
	if (!CacheProjectileClass(AttackData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CachedChargedAttackData = const_cast<UFE_ChargedProjectileAttackData*>(AttackData);
	ChargeStartTime = CombatCharacter->GetWorld() ? CombatCharacter->GetWorld()->GetTimeSeconds() : 0.0f;

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	InputReleaseTask->OnRelease.AddDynamic(this, &UFE_ChargedProjectileAttackAbility::HandleInputReleased);
	InputReleaseTask->ReadyForActivation();

	PlayAttackMontage(CombatCharacter, AttackData->ChargeMontage);
	StartLocalChargePresentation();
}

void UFE_ChargedProjectileAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (InputReleaseTask)
	{
		InputReleaseTask->EndTask();
		InputReleaseTask = nullptr;
	}
	StopLocalChargePresentation();
	CachedChargedAttackData = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFE_ChargedProjectileAttackAbility::HandleInputReleased(float TimeHeld)
{
	(void)TimeHeld;
	BeginRelease();
}

void UFE_ChargedProjectileAttackAbility::BeginRelease()
{
	if (bReleaseRequested || !CachedChargedAttackData)
	{
		return;
	}

	bReleaseRequested = true;
	ReleasedChargeAlpha = CalculateChargeAlpha();
	StopLocalChargePresentation();

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	PlayAttackMontage(CombatCharacter, CachedChargedAttackData->ReleaseMontage);
	if (CombatCharacter && CachedChargedAttackData->WeaponMeshAttackMontage)
	{
		if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
		{
			Combat->PlayWeaponMeshMontage(
				CachedChargedAttackData->WeaponMeshAttackMontage,
				NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
		}
	}
	PlayAttackEffects(CombatCharacter, GetCachedWeaponData(), CachedChargedAttackData);
	FireChargedProjectile();
}

void UFE_ChargedProjectileAttackAbility::FireChargedProjectile()
{
	if (bProjectileFired || !CachedChargedAttackData)
	{
		return;
	}

	bProjectileFired = true;
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = GetCachedWeaponData();
	if (!CombatCharacter || !WeaponData)
	{
		const bool bReplicateEndAbility = CombatCharacter && CombatCharacter->HasAuthority();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, true);
		return;
	}

	RecordProjectileFired(CombatCharacter, CachedChargedAttackData);

	// The predicting client only ends its local charge presentation. WaitInputRelease
	// forwards the release to the server, which owns the projectile spawn and then
	// replicates the final ability end back to this client.
	if (!CombatCharacter->HasAuthority())
	{
		return;
	}

	SpawnProjectile(
		CombatCharacter,
		WeaponData,
		CachedChargedAttackData,
		CalculateLaunchSpeed());
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

float UFE_ChargedProjectileAttackAbility::CalculateChargeAlpha() const
{
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UWorld* World = CombatCharacter ? CombatCharacter->GetWorld() : nullptr;
	if (!World || !CachedChargedAttackData)
	{
		return 0.0f;
	}

	const float HeldTime = FMath::Max(0.0f, World->GetTimeSeconds() - ChargeStartTime);
	return FMath::Clamp(HeldTime / FMath::Max(0.01f, CachedChargedAttackData->MaxChargeTime), 0.0f, 1.0f);
}

float UFE_ChargedProjectileAttackAbility::CalculateLaunchSpeed() const
{
	if (!CachedChargedAttackData)
	{
		return 0.0f;
	}

	return FMath::Lerp(
		FMath::Max(0.0f, CachedChargedAttackData->MinLaunchSpeed),
		FMath::Max(CachedChargedAttackData->MinLaunchSpeed, CachedChargedAttackData->MaxLaunchSpeed),
		ReleasedChargeAlpha);
}

void UFE_ChargedProjectileAttackAbility::StartLocalChargePresentation()
{
	ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!CombatCharacter || !CachedChargedAttackData || !CachedProjectileClass || !CombatCharacter->GetWorld())
	{
		return;
	}
	if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
	{
		Combat->StartChargeProjectilePresentation(
			CachedProjectileClass,
			CachedChargedAttackData->AttachmentTarget,
			CachedChargedAttackData->ChargeAttachSocketName,
			CachedChargedAttackData->ChargeAttachOffset,
			true,
			CachedChargedAttackData->MaxChargeTime,
			CachedChargedAttackData->ChargeMontage,
			CachedChargedAttackData->IsA<UFE_BowAttackData>());
		bChargePresentationActive = true;
	}
	StartSpecializedChargePresentation();
}

void UFE_ChargedProjectileAttackAbility::StopLocalChargePresentation()
{
	StopSpecializedChargePresentation();

	if (bChargePresentationActive)
	{
		if (ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UFE_CombatComponent* Combat = CombatCharacter->FindComponentByClass<UFE_CombatComponent>())
			{
				Combat->StopChargeProjectilePresentation(true);
			}
		}
		bChargePresentationActive = false;
	}

}

void UFE_ChargedProjectileAttackAbility::StartSpecializedChargePresentation()
{
}

void UFE_ChargedProjectileAttackAbility::StopSpecializedChargePresentation()
{
}

bool UFE_ChargedProjectileAttackAbility::SupportsChargedAttackData(
	const UFE_ChargedProjectileAttackData* AttackData) const
{
	(void)AttackData;
	return false;
}
