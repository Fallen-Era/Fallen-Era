#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapon/Attachment/FEWeaponAttachmentData.h"
#include "FEEquipmentInstanceData.generated.h"

USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponStat
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats", meta=(ClampMin="0.0"))
	float Offense = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Accuracy = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RecoilControl = 1.0f;
};

USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ArmorStat
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats", meta=(ClampMin="0.0"))
	float Defense = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats", meta=(ClampMin="0.0"))
	float KnockbackResistance = 0.0f;
};

/** Integration boundary: replace ItemData and adapt ResolveStats when the inventory struct is available. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponInstanceData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance")
	FGuid InstanceId;
	/** Editable DA test seed. Runtime writes affect the equipped copy, never the shared DA. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance|Ammo", meta=(ClampMin="0"))
	int32 LoadedAmmo = 0;
	/** Per-swing cost; copied into runtime equipment so inventory instances can override it independently. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance|Combat", meta=(ClampMin="0.0"))
	float StaminaCost = 10.0f;
	/** Intrinsic item stats only; do not include attachment bonuses or character buffs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance")
	FFE_WeaponStat ItemData;
	/** False uses Template.DefaultAttachments; true allows an explicitly empty loadout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance")
	bool bOverrideAttachments = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance", meta=(EditCondition="bOverrideAttachments"))
	TArray<FFE_WeaponAttachmentInstance> Attachments;

	FFE_WeaponStat ResolveStats() const { return ItemData; }
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ArmorInstanceData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance")
	FGuid InstanceId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Instance")
	FFE_ArmorStat ItemData;

	FFE_ArmorStat ResolveStats() const { return ItemData; }
	bool IsValid() const;
};
