#pragma once

#include "CoreMinimal.h"
#include "Combat/UI/Radial/FERadialMenuViewModel.h"
#include "Combat/Item/FEWeaponAmmoData.h"
#include "FEAmmoSelectionViewModel.generated.h"

class UFE_EquipmentComponent;
class UTexture2D;
struct FStreamableHandle;

USTRUCT(BlueprintType)
struct FALLENERA_API FFE_AmmoSelectionOption
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid AmmoId;
	UPROPERTY(BlueprintReadOnly) FText Name;
	UPROPERTY(BlueprintReadOnly) int32 Count = 0;
	UPROPERTY(BlueprintReadOnly) float DamageMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly) bool bLoaded = false;
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
};

/** Owner-only event-driven presentation; UI cannot mutate authoritative ammo stacks. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_AmmoSelectionViewModel : public UFE_RadialMenuViewModel
{
	GENERATED_BODY()
public:
	void Initialize(UFE_EquipmentComponent* InEquipment);
	virtual void Shutdown() override;
	virtual void SelectIndex(int32 Index) override;
	void SyncMenuItems();
	FGuid GetSelectedAmmoId() const;
	FGuid GetConfirmedAmmoId() const { return ConfirmedAmmoId; }
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Ammo") TArray<FFE_AmmoSelectionOption> Options;
private:
	UFUNCTION() void Refresh();
	UFUNCTION() void ConfirmAmmoItem(FName ItemId);
	FGuid ConfirmedAmmoId;
	TWeakObjectPtr<UFE_EquipmentComponent> Equipment;
	TSharedPtr<FStreamableHandle> LoadHandle;
	TSet<FSoftObjectPath> RequestedPaths;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> LoadedPresentation;
};
