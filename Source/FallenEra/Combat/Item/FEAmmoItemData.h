#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/Item/FEWeaponAmmoData.h"
#include "FEAmmoItemData.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AmmoTemplateData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo", meta=(Categories="Item.Ammo"))
	FGameplayTag AmmoType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo") FGameplayTag ItemTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo") TSoftObjectPtr<UTexture2D> Icon;
};

/** Shared definition plus editable test seed. Runtime consumption never mutates this asset. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_AmmoItemData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo") FFE_AmmoTemplateData Template;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo") FFE_AmmoInstanceData Instance;
};
