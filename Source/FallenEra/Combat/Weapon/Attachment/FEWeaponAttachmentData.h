#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FEWeaponAttachmentData.generated.h"

class UStaticMesh;
class USkeletalMesh;

/** Immutable definition shared by attachments; runtime item rolls never modify this asset. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_WeaponAttachmentData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attachment")
	FGameplayTag SlotTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attachment|Stats", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float AccuracyBonus = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attachment|Presentation")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Takes priority over Mesh when assigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attachment|Presentation")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Socket on the weapon mesh, not on the character. Empty attaches to the weapon origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attachment|Presentation")
	FName AttachSocketName = TEXT("OpticSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attachment|Presentation")
	FTransform SocketOffset;
};

/** Inventory integration boundary. Only the server may supply resolved item-instance stats. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_WeaponAttachmentInstance
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attachment")
	TSoftObjectPtr<UFE_WeaponAttachmentData> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attachment")
	bool bOverrideAccuracyBonus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attachment", meta=(EditCondition="bOverrideAccuracyBonus", ClampMin="-1.0", ClampMax="1.0"))
	float AccuracyBonus = 0.0f;

	float ResolveAccuracyBonus() const;
};

/** Weapon identity prevents an old replication/load completion applying parts to a new weapon. */
USTRUCT()
struct FFE_WeaponAttachmentLoadout
{
	GENERATED_BODY()
	UPROPERTY()
	FGameplayTag WeaponTag;
	UPROPERTY()
	TArray<FFE_WeaponAttachmentInstance> Attachments;
};
