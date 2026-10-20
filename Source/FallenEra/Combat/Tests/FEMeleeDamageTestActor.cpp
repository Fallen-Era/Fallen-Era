#include "Combat/Tests/FEMeleeDamageTestActor.h"

#include "Combat/Collision/FECollisionChannels.h"
#include "Components/BoxComponent.h"

AFE_MeleeDamageTestActor::AFE_MeleeDamageTestActor()
{
	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(5.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeTrace, ECR_Block);
	Box->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeAreaTrace, ECR_Overlap);
}

FFE_CombatDamageResult AFE_MeleeDamageTestActor::ReceiveCombatDamage_Implementation(const FFE_CombatDamageRequest& Request)
{
	LastRequest = Request;
	++HitCount;
	FFE_CombatDamageResult Result;
	Result.bHandled = true;
	Result.bDamageApplied = true;
	Result.AppliedDamage = FMath::Min(Health, FMath::Max(0.0f, Request.SourceAttackPower - 5.0f) * Request.HitQualityMultiplier);
	Health -= Result.AppliedDamage;
	return Result;
}

void UFE_MeleeDamageTestAbility::RunTrace(const ACharacter* Character, const UFE_WeaponItemData* Weapon,
	const UFE_WeaponAttackData* Attack) const
{
	ExecuteAttack(Character, Weapon, Attack);
}

bool UFE_ProjectileAimTestAbility::GetPreviewLaunch(const ACharacter* Character, const UFE_ProjectileAttackDataBase* Attack,
	FVector& Location, FVector& Direction) const
{
	return GetProjectileLaunchTransform(Character, Attack, EFE_ProjectileLaunchContext::LocalPreview, Location, Direction);
}
