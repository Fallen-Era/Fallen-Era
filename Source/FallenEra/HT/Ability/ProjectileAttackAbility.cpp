#include "HT/Ability/ProjectileAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HT/Character/CombatCharacter.h"
#include "HT/Weapon/WeaponItemData.h"

void UFE_ProjectileAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UFallenEraGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bInputReleased = false;
	CachedProjectileAttackData = nullptr;
	CachedProjectileClass = nullptr;
	const AFE_CombatCharacter* CombatCharacter = Cast<AFE_CombatCharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? CombatCharacter->GetCurrentWeaponData() : nullptr;
	const UFE_ProjectileAttackData* AttackData = Cast<UFE_ProjectileAttackData>(ResolveAttackData(CombatCharacter));
	if (!CombatCharacter || !WeaponData || !AttackData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CachedProjectileAttackData = const_cast<UFE_ProjectileAttackData*>(AttackData);
	CachedProjectileClass = AttackData->ProjectileClass.LoadSynchronous();
	if (!CachedProjectileClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	CacheAttackContext(WeaponData, AttackData);
	FireOnce();
	if (!AttackData->bAutomatic)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	InputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	InputReleaseTask->OnRelease.AddDynamic(this, &UFE_ProjectileAttackAbility::OnInputReleased);
	InputReleaseTask->ReadyForActivation();
	ScheduleNextShot();
}

void UFE_ProjectileAttackAbility::ExecuteAttack(
	const AFE_CombatCharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData) const
{
	const UFE_ProjectileAttackData* ProjectileAttackData = Cast<UFE_ProjectileAttackData>(AttackData);
	if (!CombatCharacter || !ProjectileAttackData || !CombatCharacter->GetWorld())
	{
		return;
	}

	UClass* ProjectileClass = CachedProjectileClass.Get();
	if (!ProjectileClass)
	{
		return;
	}

	FVector SpawnLocation = CombatCharacter->GetActorLocation();
	FRotator SpawnRotation = CombatCharacter->GetActorRotation();
	if (const USkeletalMeshComponent* Mesh = CombatCharacter->GetMesh(); Mesh && Mesh->DoesSocketExist(ProjectileAttackData->ProjectileSpawnSocketName))
	{
		SpawnLocation = Mesh->GetSocketLocation(ProjectileAttackData->ProjectileSpawnSocketName);
		SpawnRotation = Mesh->GetSocketRotation(ProjectileAttackData->ProjectileSpawnSocketName);
	}

	if (ProjectileAttackData->bUseAimDirection)
	{
		if (const AController* Controller = CombatCharacter->GetController())
		{
			FVector ViewLocation;
			Controller->GetPlayerViewPoint(ViewLocation, SpawnRotation);
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = const_cast<AFE_CombatCharacter*>(CombatCharacter);
	SpawnParameters.Instigator = const_cast<AFE_CombatCharacter*>(CombatCharacter);
	AActor* Projectile = CombatCharacter->GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParameters);
	if (Projectile)
	{
		if (UProjectileMovementComponent* MovementComponent = Projectile->FindComponentByClass<UProjectileMovementComponent>())
		{
			MovementComponent->InitialSpeed = ProjectileAttackData->InitialSpeed;
			MovementComponent->MaxSpeed = ProjectileAttackData->InitialSpeed;
			MovementComponent->ProjectileGravityScale = ProjectileAttackData->GravityScale;
			MovementComponent->Velocity = SpawnRotation.Vector() * ProjectileAttackData->InitialSpeed;
		}
	}

	(void)WeaponData;
}

void UFE_ProjectileAttackAbility::FireOnce()
{
	const AFE_CombatCharacter* CombatCharacter = Cast<AFE_CombatCharacter>(GetAvatarActorFromActorInfo());
	if (!CombatCharacter || !CachedProjectileAttackData || !CachedProjectileClass)
	{
		return;
	}

	PlayAttackMontage(CombatCharacter, CachedProjectileAttackData);
	if (!CombatCharacter->HasAuthority())
	{
		return;
	}

	ExecuteAttack(CombatCharacter, GetCachedWeaponData(), CachedProjectileAttackData);
}

void UFE_ProjectileAttackAbility::ScheduleNextShot()
{
	if (bInputReleased || !CachedProjectileAttackData)
	{
		return;
	}

	FireDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.01f, CachedProjectileAttackData->FireInterval));
	FireDelayTask->OnFinish.AddDynamic(this, &UFE_ProjectileAttackAbility::OnFireIntervalElapsed);
	FireDelayTask->ReadyForActivation();
}

void UFE_ProjectileAttackAbility::OnFireIntervalElapsed()
{
	if (!bInputReleased && IsActive())
	{
		FireOnce();
		ScheduleNextShot();
	}
}

void UFE_ProjectileAttackAbility::OnInputReleased(float TimeHeld)
{
	(void)TimeHeld;
	bInputReleased = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
