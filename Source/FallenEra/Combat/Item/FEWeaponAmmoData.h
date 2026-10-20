#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FEWeaponAmmoData.generated.h"

class UAnimMontage;

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
struct FALLENERA_API FFE_AmmoStack
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo", meta=(Categories="Item.Ammo"))
	FGameplayTag AmmoType;
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
};
