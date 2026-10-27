#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/UI/Radial/FERadialMenuInput.h"
#include "FERadialMenuWidget.generated.h"

class UFE_RadialMenuViewModel;
class UFE_RadialMenuOptionWidget;
class UCanvasPanel;
class USizeBox;
class UTextBlock;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFE_RadialMenuCancelled);

/** Reusable passive UMG wheel. The owning interaction decides which key confirms/closes it. */
UCLASS(Abstract, Blueprintable)
class FALLENERA_API UFE_RadialMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Radial Menu") bool SetMenuModel(UFE_RadialMenuViewModel* InModel);
	UFUNCTION(BlueprintPure, Category="Radial Menu") UFE_RadialMenuViewModel* GetMenuModel() const { return Model; }
	/** Optional owner-local input lease for BP callers; ammo GA already owns its own lease. */
	UFUNCTION(BlueprintCallable, Category="Radial Menu") bool CaptureMenuInput();
	UFUNCTION(BlueprintCallable, Category="Radial Menu") bool ConfirmMenu();
	UFUNCTION(BlueprintCallable, Category="Radial Menu") void CancelMenu();
	FSimpleDelegate CancelRequested;
	UPROPERTY(BlueprintAssignable, Category="Radial Menu") FFE_RadialMenuCancelled OnCancelRequested;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Radial Menu") TSoftClassPtr<UFE_RadialMenuOptionWidget> OptionWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Radial Menu", meta=(ClampMin="1.0")) float WheelSize = 500.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Radial Menu", meta=(ClampMin="0.0")) float InnerRadius = 85.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Radial Menu", meta=(ClampMin="1.0")) float OuterRadius = 225.0f;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& Event) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void UpdateSelectedText(const FText& Text);
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USizeBox> SizeBox_Wheel;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> CanvasPanel_Options;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Title;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Hint;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SelectedItem;
private:
	void RefreshPresentation();
	void RequestCancel();
	FFE_RadialMenuInput InputLease;
	bool bRequestingCancel = false;
	bool bClosingAfterConfirm = false;
	UPROPERTY(Transient) TObjectPtr<UFE_RadialMenuViewModel> Model;
	UPROPERTY(Transient) TArray<TObjectPtr<UFE_RadialMenuOptionWidget>> Entries;
	FDelegateHandle ValuesHandle;
};
