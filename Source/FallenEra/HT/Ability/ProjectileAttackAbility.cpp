#include "HT/Ability/ProjectileAttackAbility.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "HT/Component/EquipmentComponent.h"
#include "HT/Projectile/CombatProjectile.h"
#include "HT/Weapon/WeaponItemData.h"
#include "Kismet/GameplayStatics.h"

void UFE_ProjectileAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UFallenEraGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UFE_WeaponItemData* WeaponData = CombatCharacter ? GetWeaponData(CombatCharacter) : nullptr;
	const UFE_ProjectileAttackData* AttackData = Cast<UFE_ProjectileAttackData>(ResolveAttackData(CombatCharacter));
	if (!CombatCharacter || !WeaponData || !AttackData ||
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

	RecordProjectileFired(CombatCharacter, AttackData);
	PlayAttackMontage(CombatCharacter, AttackData);
	if (CombatCharacter->HasAuthority())
	{
		ExecuteAttack(CombatCharacter, WeaponData, AttackData);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UFE_ProjectileAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	CachedProjectileClass = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFE_ProjectileAttackAbility::ExecuteAttack(
	const ACharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_WeaponAttackData* AttackData) const
{
	const UFE_ProjectileAttackData* ProjectileAttackData = Cast<UFE_ProjectileAttackData>(AttackData);
	if (ProjectileAttackData)
	{
		SpawnProjectile(CombatCharacter, WeaponData, ProjectileAttackData, ProjectileAttackData->InitialSpeed);
	}
}

bool UFE_ProjectileAttackAbility::CacheProjectileClass(const UFE_ProjectileAttackDataBase* AttackData)
{
	CachedProjectileClass = AttackData ? AttackData->ProjectileClass.Get() : nullptr;
	return CachedProjectileClass != nullptr;
}

bool UFE_ProjectileAttackAbility::CanFireProjectile(
	const ACharacter* CombatCharacter,
	const UFE_ProjectileAttackDataBase* AttackData) const
{
	const UWorld* World = CombatCharacter ? CombatCharacter->GetWorld() : nullptr;
	return World && AttackData && World->GetTimeSeconds() >= NextFireTime;
}

void UFE_ProjectileAttackAbility::RecordProjectileFired(
	const ACharacter* CombatCharacter,
	const UFE_ProjectileAttackDataBase* AttackData)
{
	if (const UWorld* World = CombatCharacter ? CombatCharacter->GetWorld() : nullptr; World && AttackData)
	{
		NextFireTime = World->GetTimeSeconds() + FMath::Max(0.01f, AttackData->FireInterval);
	}
}

bool UFE_ProjectileAttackAbility::SpawnProjectile(
	const ACharacter* CombatCharacter,
	const UFE_WeaponItemData* WeaponData,
	const UFE_ProjectileAttackDataBase* AttackData,
	float LaunchSpeed) const
{
	if (!CombatCharacter || !CombatCharacter->HasAuthority() || !WeaponData || !AttackData ||
		!CachedProjectileClass || !CombatCharacter->GetWorld())
	{
		return false;
	}

	FVector SpawnLocation;
	FVector LaunchDirection;
	if (!GetProjectileLaunchTransform(
		CombatCharacter,
		AttackData,
		EFE_ProjectileLaunchContext::Authority,
		SpawnLocation,
		LaunchDirection))
	{
		return false;
	}

	const FTransform SpawnTransform(LaunchDirection.Rotation(), SpawnLocation);
	AFE_CombatProjectile* Projectile = CombatCharacter->GetWorld()->SpawnActorDeferred<AFE_CombatProjectile>(
		CachedProjectileClass,
		SpawnTransform,
		const_cast<ACharacter*>(CombatCharacter),
		const_cast<ACharacter*>(CombatCharacter),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return false;
	}

	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
	Projectile->InitializeProjectile(
		const_cast<ACharacter*>(CombatCharacter),
		WeaponData,
		AttackData,
		GetCachedDamageEffectClass(),
		LaunchDirection * FMath::Max(0.0f, LaunchSpeed),
		AttackData->GravityScale);
	return true;
}

bool UFE_ProjectileAttackAbility::GetProjectileLaunchTransform(
	const ACharacter* CombatCharacter,
	const UFE_ProjectileAttackDataBase* AttackData,
	EFE_ProjectileLaunchContext LaunchContext,
	FVector& OutLocation,
	FVector& OutDirection) const
{
	if (!CombatCharacter || !AttackData)
	{
		return false;
	}

	OutLocation = CombatCharacter->GetActorLocation();
	FRotator LaunchRotation = CombatCharacter->GetActorRotation();
	const UFE_EquipmentComponent* Equipment = CombatCharacter->FindComponentByClass<UFE_EquipmentComponent>();
	const UMeshComponent* WeaponMesh = nullptr;
	if (Equipment)
	{
		// Authoritative projectiles use the replicated world presentation. Local previews
		// use the first-person weapon when one exists so the arc starts at the visible muzzle.
		WeaponMesh = LaunchContext == EFE_ProjectileLaunchContext::LocalPreview
			? Equipment->GetEquippedFirstPersonWeaponMesh()
			: Equipment->GetEquippedWorldWeaponMesh();
		if (!WeaponMesh)
		{
			WeaponMesh = Equipment->GetEquippedWorldWeaponMesh();
		}
	}

	if (WeaponMesh && WeaponMesh->DoesSocketExist(AttackData->ProjectileSpawnSocketName))
	{
		OutLocation = WeaponMesh->GetSocketLocation(AttackData->ProjectileSpawnSocketName);
		LaunchRotation = WeaponMesh->GetSocketRotation(AttackData->ProjectileSpawnSocketName);
	}
	else if (const USkeletalMeshComponent* Mesh = CombatCharacter->GetMesh();
		Mesh && Mesh->DoesSocketExist(AttackData->ProjectileSpawnSocketName))
	{
		OutLocation = Mesh->GetSocketLocation(AttackData->ProjectileSpawnSocketName);
		LaunchRotation = Mesh->GetSocketRotation(AttackData->ProjectileSpawnSocketName);
	}

	if (AttackData->bUseAimDirection)
	{
		if (const AController* Controller = CombatCharacter->GetController())
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
			FVector AimPoint = ViewLocation + ViewRotation.Vector() * FMath::Max(0.0f, AttackData->AimTraceRange);
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FE_ProjectileAim), true, CombatCharacter);
			FHitResult AimHit;
			if (CombatCharacter->GetWorld() && CombatCharacter->GetWorld()->LineTraceSingleByChannel(
				AimHit, ViewLocation, AimPoint, TraceChannel, QueryParams))
			{
				AimPoint = AimHit.ImpactPoint;
			}
			LaunchRotation = (AimPoint - OutLocation).Rotation();
		}
	}

	OutDirection = LaunchRotation.Vector().GetSafeNormal();
	return !OutDirection.IsNearlyZero();
}
