#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatusWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
class UFallenEraAttributeSet;
struct FOnAttributeChangeData;

/** UMG base for W_PlayerStatusWidget. Bind the four named widgets in its Blueprint child. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_PlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Rebinds the widget to the owning PlayerState's ASC when possession/replication finishes. */
	UFUNCTION(BlueprintCallable, Category="Player Status")
	void RefreshFromPlayerState();

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> ProgressBar_HPBar;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_HPText;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> ProgressBar_StaminaBar;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_StaminaText;

private:
	void BindToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent, const UFallenEraAttributeSet* AttributeSet);
	void UnbindFromAbilitySystem();
	void RefreshAllValues(const UFallenEraAttributeSet* AttributeSet) const;
	void RefreshHealth(float Health, float MaxHealth) const;
	void RefreshStamina(float Stamina, float MaxStamina) const;

	void OnHealthChanged(const FOnAttributeChangeData& ChangeData);
	void OnMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
	void OnStaminaChanged(const FOnAttributeChangeData& ChangeData);
	void OnMaxStaminaChanged(const FOnAttributeChangeData& ChangeData);

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	TWeakObjectPtr<const UFallenEraAttributeSet> BoundAttributeSet;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;
};
