#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FEWeaponAmmoData.generated.h"

class UAnimMontage;
class UFE_AmmoItemData;

USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AmmoStat
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo", meta=(ClampMin="0.0"))
	float DamageMultiplier = 1.0f;
};

/** Inventory integration boundary. Never contains character buffs or weapon stats. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AmmoInstanceData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo") FGuid InstanceId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo") FFE_AmmoStat ItemData;
	FFE_AmmoStat ResolveStats() const { return ItemData; }
};

/** Value snapshot retained in a magazine and copied into a successful shot. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AmmoRoundData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo", meta=(Categories="Item.Ammo"))
	FGameplayTag AmmoType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	TSoftObjectPtr<UFE_AmmoItemData> AmmoData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	FFE_AmmoInstanceData Instance;
	bool HasSameStats(const FFE_AmmoRoundData& Other) const
	{
		return AmmoType == Other.AmmoType && AmmoData == Other.AmmoData &&
			Instance.ItemData.DamageMultiplier == Other.Instance.ItemData.DamageMultiplier;
	}
};

/** One exact ammo tag per weapon; sibling/parent tags are not interchangeable. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponAmmoSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	bool bUsesAmmo = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo", meta=(Categories="Item.Ammo", EditCondition="bUsesAmmo"))
	FGameplayTag AmmoType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo", meta=(ClampMin="1", EditCondition="bUsesAmmo"))
	int32 MagazineCapacity = 1;
	/** Reload completion uses montage length / RateScale. Charged attacks refill instantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	TSoftObjectPtr<UAnimMontage> ReloadMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	TSoftObjectPtr<UAnimMontage> WeaponReloadMontage;
};

/** Loose rounds only: ammo already loaded in a weapon is not counted here. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AmmoStack : public FFE_AmmoRoundData
{
	GENERATED_BODY()
	/** Use the DA's instance as a test seed; disable to supply generated instance stats below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	bool bUseDefinitionStats = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo", meta=(ClampMin="0"))
	int32 Count = 0;
};

/** Compact owning-client view. The authoritative loaded count lives in the equipped item instance. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponAmmoState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Ammo") FGameplayTag WeaponTag;
	UPROPERTY(BlueprintReadOnly, Category="Ammo") FGameplayTag AmmoType;
	UPROPERTY(BlueprintReadOnly, Category="Ammo") int32 LoadedAmmo = 0;
	UPROPERTY(BlueprintReadOnly, Category="Ammo") int32 ReserveAmmo = 0;
	UPROPERTY(BlueprintReadOnly, Category="Ammo") bool bReloading = false;
	UPROPERTY(BlueprintReadOnly, Category="Ammo") FGuid WeaponInstanceId;
	UPROPERTY(BlueprintReadOnly, Category="Ammo") FGuid SelectedAmmoId;
	UPROPERTY(BlueprintReadOnly, Category="Ammo") FFE_AmmoRoundData LoadedAmmoData;
};
