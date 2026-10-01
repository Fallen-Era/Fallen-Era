#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "ImpactEffectData.generated.h"

class UNiagaraSystem;
class UParticleSystem;
class USoundBase;

/** VFX and audio presentation for one physical surface. Niagara takes priority over Particle. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ImpactEffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact")
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	/** Legacy Cascade fallback used only when NiagaraSystem is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact")
	TObjectPtr<UParticleSystem> ParticleSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact")
	TObjectPtr<USoundBase> Sound;

	/** Shared scale for either Niagara or Particle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|VFX", meta=(DisplayName="VFX Scale"))
	FVector NiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Sound", meta=(ClampMin="0.0"))
	float SoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Sound", meta=(ClampMin="0.0"))
	float SoundPitch = 1.0f;
};

/** Weapon-owned lookup table used by GameplayCue.Impact. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_ImpactEffectData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FFE_ImpactEffectEntry* FindEffectForSurface(EPhysicalSurface SurfaceType) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact")
	TArray<FFE_ImpactEffectEntry> SurfaceEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback")
	TObjectPtr<UNiagaraSystem> DefaultNiagaraSystem;

	/** Legacy Cascade fallback used only when DefaultNiagaraSystem is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback")
	TObjectPtr<UParticleSystem> DefaultParticleSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback")
	TObjectPtr<USoundBase> DefaultSound;

	/** Shared scale for either default Niagara or Particle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback|VFX", meta=(DisplayName="Default VFX Scale"))
	FVector DefaultNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback|Sound", meta=(ClampMin="0.0"))
	float DefaultSoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback|Sound", meta=(ClampMin="0.0"))
	float DefaultSoundPitch = 1.0f;

	friend class UFE_ImpactGameplayCue;
};
