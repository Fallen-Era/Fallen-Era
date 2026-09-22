#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "ImpactEffectData.generated.h"

class UNiagaraSystem;
class USoundBase;

/** Niagara and audio presentation for one physical surface. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ImpactEffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact")
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Niagara")
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback")
	TObjectPtr<USoundBase> DefaultSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback|Niagara")
	FVector DefaultNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback|Sound", meta=(ClampMin="0.0"))
	float DefaultSoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Impact|Fallback|Sound", meta=(ClampMin="0.0"))
	float DefaultSoundPitch = 1.0f;

	friend class UFE_ImpactGameplayCue;
};
