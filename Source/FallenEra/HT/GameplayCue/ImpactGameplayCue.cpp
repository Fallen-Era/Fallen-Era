#include "HT/GameplayCue/ImpactGameplayCue.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Engine/World.h"
#include "HT/Impact/ImpactEffectData.h"
#include "HT/Weapon/WeaponItemData.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UFE_ImpactGameplayCue::UFE_ImpactGameplayCue()
{
	GameplayCueTag = FallenEraGameplayTags::GameplayCue_Impact;
	GameplayCueName = GameplayCueTag.GetTagName();
}

bool UFE_ImpactGameplayCue::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	const UObject* SourceObject = Parameters.GetSourceObject();
	const UFE_WeaponAttackData* AttackData = Cast<UFE_WeaponAttackData>(SourceObject);
	const UFE_WeaponItemData* WeaponData = AttackData
		? Cast<UFE_WeaponItemData>(AttackData->GetOuter())
		: Cast<UFE_WeaponItemData>(SourceObject);
	const UFE_ImpactEffectData* ImpactData = WeaponData ? WeaponData->ImpactEffectData : nullptr;
	if (!ImpactData)
	{
		return false;
	}

	const FHitResult* HitResult = Parameters.EffectContext.GetHitResult();

	// Do not use a ternary here: FGameplayCueParameters stores quantized vector
	// types, while FHitResult stores regular FVector values. Explicit assignments
	// avoid conditional-operator type conversion errors in MSVC.
	FVector ImpactPointVector = FVector::ZeroVector;
	FVector ImpactNormal = FVector::ZeroVector;
	if (HitResult)
	{
		ImpactPointVector = HitResult->ImpactPoint;
		ImpactNormal = HitResult->ImpactNormal;
	}
	else
	{
		ImpactPointVector = FVector(Parameters.Location);
		ImpactNormal = FVector(Parameters.Normal);
	}
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector::UpVector;
	}

	const UPhysicalMaterial* PhysicalMaterial = Parameters.PhysicalMaterial.Get();
	if (!PhysicalMaterial && HitResult)
	{
		PhysicalMaterial = HitResult->PhysMaterial.Get();
	}
	const EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(PhysicalMaterial);
	const FFE_ImpactEffectEntry* SurfaceEffect = ImpactData->FindEffectForSurface(SurfaceType);

	UNiagaraSystem* NiagaraSystem = SurfaceEffect && SurfaceEffect->NiagaraSystem
		? SurfaceEffect->NiagaraSystem.Get()
		: ImpactData->DefaultNiagaraSystem.Get();
	const FVector NiagaraScale = SurfaceEffect
		? SurfaceEffect->NiagaraScale
		: ImpactData->DefaultNiagaraScale;
	if (NiagaraSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			NiagaraSystem,
			ImpactPointVector,
			ImpactNormal.ToOrientationRotator(),
			NiagaraScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}

	USoundBase* Sound = SurfaceEffect && SurfaceEffect->Sound
		? SurfaceEffect->Sound.Get()
		: ImpactData->DefaultSound.Get();
	const float SoundVolume = SurfaceEffect
		? SurfaceEffect->SoundVolume
		: ImpactData->DefaultSoundVolume;
	const float SoundPitch = SurfaceEffect
		? SurfaceEffect->SoundPitch
		: ImpactData->DefaultSoundPitch;
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Sound, ImpactPointVector, SoundVolume, SoundPitch);
	}

	return NiagaraSystem != nullptr || Sound != nullptr;
}
