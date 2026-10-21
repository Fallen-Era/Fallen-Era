#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Damage/FEHitZone.h"
#include "FEHitZoneMappingData.generated.h"

class USkeletalMesh;

/** Skeleton-specific rules; no damage values are repeated for individual bones. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_HitZoneMappingData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	TMap<FName, EFE_HitZone> BoneZones;

	/** Build once per mesh; receivers sharing this asset reuse the same resolved rules. */
	void PrepareForMesh(USkeletalMesh* Mesh);
	bool IsPreparedForMesh(USkeletalMesh* Mesh) const;
	/** Exact rules override inherited rules. Never builds a cache on the hit path. */
	const EFE_HitZone* FindHitZone(FName BoneName, USkeletalMesh* Mesh = nullptr) const;
	static FName GetZoneName(EFE_HitZone Zone);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif

private:
	// Weak keys must not keep otherwise unused character meshes loaded.
	TMap<TWeakObjectPtr<USkeletalMesh>, TMap<FName, EFE_HitZone>> ResolvedBoneZonesByMesh;
};
