#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "FERadialMenuViewModel.generated.h"

class UTexture2D;
struct FStreamableHandle;
DECLARE_DYNAMIC_DELEGATE_OneParam(FFE_RadialMenuAction, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFE_RadialMenuExecuted, FName, ItemId);

/** Presentation and local command binding. Authoritative gameplay must still validate its own requests. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_RadialMenuItem
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText SecondaryText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DetailText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText MarkerText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bMarked = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFE_RadialMenuAction OnExecute;
};

/** No inventory/equipment dependency. Hover is passive; ConfirmSelection executes at most once per interaction. */
UCLASS(BlueprintType)
class FALLENERA_API UFE_RadialMenuViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Radial Menu") bool SetItems(const TArray<FFE_RadialMenuItem>& InItems);
	UFUNCTION(BlueprintCallable, Category="Radial Menu") virtual void SelectIndex(int32 Index);
	UFUNCTION(BlueprintCallable, Category="Radial Menu") bool ConfirmSelection();
	UFUNCTION(BlueprintCallable, Category="Radial Menu") void BeginInteraction();
	UFUNCTION(BlueprintCallable, Category="Radial Menu") void SetMenuText(FText InTitle, FText InConfirmHint, FText InCancelHint, FText InEmptyText);
	virtual void Shutdown();
	virtual void BeginDestroy() override;
	FSimpleMulticastDelegate ValuesChanged;
	UPROPERTY(BlueprintAssignable, Category="Radial Menu") FFE_RadialMenuExecuted OnItemExecuted;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Radial Menu") TArray<FFE_RadialMenuItem> Items;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Radial Menu") int32 SelectedIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Radial Menu") FText Title;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Radial Menu") FText ConfirmHint;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Radial Menu") FText CancelHint;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Radial Menu") FText EmptyText;
	/** Clockwise from twelve o'clock. Center/outside are cancellation zones. */
	static int32 FindSector(FVector2D Offset, int32 Count, float InnerRadius, float OuterRadius);
private:
	void FinishLoadingIcons();
	TSharedPtr<FStreamableHandle> IconLoadHandle;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> LoadedIcons;
	bool bConfirmed = false;
};
