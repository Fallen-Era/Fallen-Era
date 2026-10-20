#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FEPlayerStatusWidget.generated.h"

class UHorizontalBox;
class UFE_PlayerStatusViewModel;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UAbilitySystemComponent;
class UFallenEraAttributeSet;
class UFE_CharacterStatusComponent;
class UFE_ConditionSlotWidget;
struct FOnAttributeChangeData;

/** Event-driven status display; named widgets are bound by the Blueprint child. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_PlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFE_PlayerStatusWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Rebinds the widget to the owning PlayerState's ASC when possession/replication finishes. */
	UFUNCTION(BlueprintCallable, Category="Player Status")
	void RefreshFromPlayerState();

	UPROPERTY(Transient, BlueprintReadOnly, Category="Player Status")
	TObjectPtr<UFE_PlayerStatusViewModel> StatusViewModel;

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> ProgressBar_HPBar;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_HPText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> ProgressBar_StaminaBar;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_StaminaText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHorizontalBox> HorizontalBox_AmmoStatusBox;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_LoadedAmmo;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_ReserveAmmo;

	/** Keep this exact name in W_PlayerStatusWidget. Optional allows old widget assets to load during migration. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> VerticalBox_ConditionBox;

	/** Set this to W_ConditionSlotWidget; the native slot class is used as a functional fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player Status|Conditions")
	TSubclassOf<UFE_ConditionSlotWidget> ConditionSlotWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category="Player Status|Stamina Warning", meta=(ClampMin="0.02", Units="s"))
	float StaminaBlinkInterval = 0.12f;
	UPROPERTY(EditDefaultsOnly, Category="Player Status|Stamina Warning", meta=(ClampMin="1", ClampMax="10"))
	int32 StaminaBlinkCount = 3;
	UPROPERTY(EditDefaultsOnly, Category="Player Status|Stamina Warning", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StaminaBlinkDimOpacity = 0.2f;

private:
	void BlinkStaminaBar();
	void UpdateStaminaBlink();
	void StopStaminaBlink();
	FTimerHandle StaminaBlinkTimer;
	float OriginalStaminaBarOpacity = 1.0f;
	int32 StaminaBlinkStep = 0;
	bool bStaminaBlinking = false;
	FLinearColor OriginalStaminaBarColor = FLinearColor::White;
	bool bStaminaBarColorCached = false;
	void RefreshValues();
	void RefreshAmmo();
	void RefreshHealth(float Health, float MaxHealth) const;
	void RefreshStamina(float Stamina, float MaxStamina) const;
	void RebuildConditionSlots();
	FDelegateHandle ValuesChangedHandle;
	FDelegateHandle ConditionsChangedHandle;
	FDelegateHandle AmmoChangedHandle;
	FDelegateHandle StaminaRejectedHandle;
	TMap<FGameplayTag, double> ConditionEndTimes;
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UFE_ConditionSlotWidget>> ConditionSlots;
};
