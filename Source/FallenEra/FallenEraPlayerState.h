#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "FallenEraPlayerState.generated.h"

class UFallenEraAbilitySet;
class UFallenEraAbilitySystemComponent;
class UFallenEraAttributeSet;

/** Persistent owner of the player's ASC. The possessed Character is assigned as its Avatar. */
UCLASS()
class FALLENERA_API AFallenEraPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFallenEraPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category="Abilities")
	UFallenEraAbilitySystemComponent* GetFallenEraAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category="Abilities|Attributes")
	const UFallenEraAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/** Called by the pawn after possession/PlayerState replication. Safe to call more than once. */
	void InitializeAbilitySystem(AActor* AvatarActor);

	UFUNCTION(BlueprintPure, Category="Abilities|Tags")
	bool HasStateTag(FGameplayTag StateTag) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Abilities|Tags")
	void AddStateTag(FGameplayTag StateTag);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Abilities|Tags")
	void RemoveStateTag(FGameplayTag StateTag);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities")
	TObjectPtr<UFallenEraAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities")
	TObjectPtr<UFallenEraAttributeSet> AttributeSet;

	/** Granted once on the server and retained across pawn respawns. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Abilities")
	TArray<TObjectPtr<UFallenEraAbilitySet>> DefaultAbilitySets;
};
