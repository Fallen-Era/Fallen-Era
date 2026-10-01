#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CharacterConditionTypes.generated.h"

class UGameplayEffect;
class UTexture2D;

UENUM(BlueprintType)
enum class EFE_ConditionExpirationBehavior : uint8
{
	Remove,
	KillOwner
};

/** A condition and its independent application probability, owned by the damage source. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ConditionApplicationChance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition", meta=(Categories="State.Condition"))
	FGameplayTag ConditionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Chance = 0.0f;
};

/** Minimal replicated state used by the owning client's condition UI. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_ActiveCharacterCondition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Condition")
	FGameplayTag ConditionTag;

	/** Synchronized server-world time at which this condition ends. Zero means infinite. */
	UPROPERTY(BlueprintReadOnly, Category="Condition")
	double EndServerWorldTime = 0.0;
};

/** Data that defines one buff/debuff managed by the character status component. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_CharacterConditionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition", meta=(Categories="State.Condition"))
	FGameplayTag ConditionTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition|UI")
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition|UI", meta=(ClampMin="1.0"))
	FVector2D IconSize = FVector2D(32.0, 32.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition|Lifetime", meta=(ClampMin="0.0", Units="s"))
	float Duration = 0.0f;

	/** Zero disables periodic health loss. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition|Periodic", meta=(ClampMin="0.0", Units="s"))
	float TickInterval = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition|Periodic", meta=(ClampMin="0.0"))
	float HealthLossPerTick = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition|Lifetime")
	EFE_ConditionExpirationBehavior ExpirationBehavior = EFE_ConditionExpirationBehavior::Remove;

	/** Optional GAS effect for additional modifiers/cues. The component removes it with the condition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition|GAS")
	TSubclassOf<UGameplayEffect> AppliedEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition|Application")
	bool bRefreshDurationOnReapply = true;
};
