#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "UObject/Interface.h"
#include "Damageable.generated.h"

class UGameplayEffect;
class UFE_WeaponAttackData;

/** Immutable, server-local description of one combat hit. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_CombatDamageRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	TObjectPtr<UFE_WeaponAttackData> AttackData;

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	float SourceAttackPower = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	bool bHasHitResult = false;
};

/** Lets the caller distinguish a handled hit from the mechanism used to handle it. */
USTRUCT(BlueprintType)
struct FALLENERA_API FFE_CombatDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	bool bHandled = false;

	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	bool bDamageApplied = false;

	/** True when a successfully applied GameplayEffect already owns the impact cue. */
	UPROPERTY(BlueprintReadOnly, Category="FallenEra|Combat|Damage")
	bool bImpactCueHandled = false;
};

/** Common damage entry point. Each receiver chooses GAS, local health, or another damage model. */
UINTERFACE(BlueprintType)
class FALLENERA_API UFE_Damageable : public UInterface
{
	GENERATED_BODY()
};

class FALLENERA_API IFE_Damageable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="FallenEra|Combat|Damage")
	FFE_CombatDamageResult ReceiveCombatDamage(const FFE_CombatDamageRequest& DamageRequest);
};
