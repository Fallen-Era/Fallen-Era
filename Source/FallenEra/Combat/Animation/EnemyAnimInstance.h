#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Combat/AI/AISettings.h"
#include "EnemyAnimInstance.generated.h"

class AFE_EnemyCharacter;
class UBlendSpace;

/** AnimBP base that imports the selected character AI preset once it is cached at BeginPlay. */
UCLASS(Blueprintable, BlueprintType)
class FALLENERA_API UFE_EnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Animation")
	FSAISettings GetAISettings() const { return AISettings; }

protected:
	UPROPERTY(BlueprintReadOnly, Transient, Category="FallenEra|AI|Animation")
	FSAISettings AISettings;

	/** Expose the Blend Space asset pin on the AnimGraph player and connect this variable. */
	UPROPERTY(BlueprintReadOnly, Transient, Category="FallenEra|AI|Animation")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace;

private:
	void TryCacheAISettings();

	TWeakObjectPtr<AFE_EnemyCharacter> EnemyCharacter;
	bool bSettingsCached = false;
};
