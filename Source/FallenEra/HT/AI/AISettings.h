#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AISettings.generated.h"

class UAnimMontage;
class UBlendSpace;
class UGameplayEffect;

/** Montage-specific melee trace settings so each attack can use different weapon sockets. */
USTRUCT(BlueprintType)
struct FALLENERA_API FSAIAttackMontageSettings
{
	GENERATED_BODY()

	FSAIAttackMontageSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FName TraceStartSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace")
	FName TraceEndSocket = TEXT("hand_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(ClampMin="0.0"))
	float TraceRadius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(ClampMin="1"))
	int32 MaxHitTargets = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(Categories="GameplayEvent"))
	FGameplayTag TraceStartEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(Categories="GameplayEvent"))
	FGameplayTag TraceEndEventTag;

	/** Ends the attack lock. Montage length remains the bounded fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Trace", meta=(Categories="GameplayEvent"))
	FGameplayTag AttackResetEventTag;
};

/** Complete reusable configuration cached by an enemy at BeginPlay. */
USTRUCT(BlueprintType)
struct FALLENERA_API FSAISettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(TitleProperty="AttackMontage"))
	TArray<FSAIAttackMontageSettings> AttackMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.01"))
	float AttackCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Reaction")
	TArray<TObjectPtr<UAnimMontage>> HitReactionMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float PatrolMovementSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="0.0"))
	float ChaseMovementSpeed = 450.0f;
};
