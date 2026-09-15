#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "CombatComponent.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;

/** Reusable, server-authoritative bridge for applying GameplayEffect damage to any GAS actor. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_CombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFE_CombatComponent();

	/** Applies Damage to TargetActor through the configured instant GameplayEffect. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat")
	bool ApplyDamage(AActor* TargetActor, float DamageAmount);

	/** Applies a custom GameplayEffect and supplies the configured SetByCaller damage tag. */
	UFUNCTION(BlueprintCallable, Category="FallenEra|Combat")
	bool ApplyDamageWithEffect(AActor* TargetActor, float DamageAmount, TSubclassOf<UGameplayEffect> DamageEffectClass);

	/** Returns the ASC exposed by the actor, including PlayerState-owned ASCs. */
	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	static UAbilitySystemComponent* FindAbilitySystemComponent(AActor* Actor);

protected:
	/** Override this per character or weapon when a different damage GE is required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat", meta=(AllowedClasses="/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat", meta=(Categories="SetByCaller"))
	FGameplayTag DamageSetByCallerTag;

private:
	bool ApplyDamageInternal(AActor* TargetActor, float DamageAmount, TSubclassOf<UGameplayEffect> EffectClass);
};
