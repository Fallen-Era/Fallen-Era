#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "FEConditionSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/** Displays one condition icon and its server-synchronized remaining time. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_ConditionSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFE_ConditionSlotWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="FallenEra|Status")
	void InitializeConditionSlot(
		FGameplayTag InConditionTag,
		UTexture2D* InIconTexture,
		FVector2D InIconSize,
		FText InDisplayName,
		double InEndServerWorldTime);

	UFUNCTION(BlueprintPure, Category="FallenEra|Status")
	FGameplayTag GetConditionTag() const { return ConditionTag; }

protected:
	/** Use these exact names in a W_ConditionSlotWidget Blueprint to replace the native fallback layout. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_ConditionIcon;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RemainingTime;

	/** Localizable single-argument format, for example "{0}분" or "{0}m". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player Status|Conditions")
	FText MinuteTextFormat;

	/** Localizable single-argument format, for example "{0}초" or "{0}s". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player Status|Conditions")
	FText SecondTextFormat;

	/** Text inserted between the formatted minute and second parts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player Status|Conditions")
	FText TimePartSeparator;

private:
	void CreateFallbackLayout();
	void RefreshVisuals();
	void RefreshRemainingTime();
	FText FormatRemainingTime(int32 RemainingSeconds) const;
	double GetServerWorldTime() const;

	FGameplayTag ConditionTag;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> IconTexture;
	FVector2D IconSize = FVector2D(32.0, 32.0);
	FText DisplayName;
	double EndServerWorldTime = 0.0;
	int32 LastDisplayedSeconds = TNumericLimits<int32>::Lowest();
};
