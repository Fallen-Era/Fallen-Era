// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "HT/Interface/CombatPresentation.h"
#include "HT/Interface/Damageable.h"
#include "Logging/LogMacros.h"
#include "FallenEraCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UAbilitySystemComponent;
class UAnimMontage;
class UFallenEraAbilitySystemComponent;
class UFE_CombatComponent;
class UFE_EquipmentComponent;
class UFE_CharacterStatusComponent;
class UNavigationInvokerComponent;
class UAIPerceptionStimuliSourceComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

USTRUCT(BlueprintType)
struct FALLENERA_API FFallenEraAbilityInputBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(Categories="Ability.Input"))
	FGameplayTag InputTag;
};

/**
 *  A basic first person character
 */
UCLASS(Abstract)
class AFallenEraCharacter : public ACharacter, public IAbilitySystemInterface,
	public IFE_CombatPresentation, public IFE_Damageable, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Extra Enhanced Input actions routed to granted GameplayAbilities by InputTag. */
	UPROPERTY(EditAnywhere, Category="Input|Abilities")
	TArray<FFallenEraAbilityInputBinding> AbilityInputBindings;
	
public:
	AFallenEraCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UFallenEraAbilitySystemComponent* GetFallenEraAbilitySystemComponent() const;
	virtual USkeletalMeshComponent* GetCombatFirstPersonMesh() const override { return GetFirstPersonMesh(); }
	virtual UCameraComponent* GetCombatCamera() const override { return GetFirstPersonCameraComponent(); }
	virtual FFE_CombatDamageResult ReceiveCombatDamage_Implementation(
		const FFE_CombatDamageRequest& DamageRequest) override;
	virtual FGenericTeamId GetGenericTeamId() const override;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat")
	UFE_CombatComponent* GetCombatComponent() const { return CombatComponent; }

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	UFUNCTION(BlueprintPure, Category="FallenEra|Combat|Equipment")
	UFE_EquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UFUNCTION(BlueprintPure, Category="FallenEra|Status")
	UFE_CharacterStatusComponent* GetCharacterStatusComponent() const { return CharacterStatusComponent; }

	/** Plays an attack montage on the character's world and first-person meshes. */
	void PlayAttackMontage(UAnimMontage* Montage);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttackMontage(UAnimMontage* Montage);

	/** Shared damage/effect entry point used by player abilities and weapons. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UFE_CombatComponent> CombatComponent;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	int32 CombatMappingPriority = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	TObjectPtr<UInputAction> CombatLeftClickAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	TObjectPtr<UInputAction> CombatRightClickAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FallenEra|Combat|Input")
	TObjectPtr<UInputAction> CombatSwapAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Combat|Equipment", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UFE_EquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|Status", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UFE_CharacterStatusComponent> CharacterStatusComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|AI", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAIPerceptionStimuliSourceComponent> PerceptionStimuliSourceComponent;

	/** Generates navigation around the moving player in invoker-based open worlds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallenEra|AI|Navigation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNavigationInvokerComponent> NavigationInvokerComponent;

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	void AbilityInputPressed(const FInputActionValue& Value, FGameplayTag InputTag);
	void AbilityInputReleased(const FInputActionValue& Value, FGameplayTag InputTag);
	void HandleCombatLeftClickStarted(const FInputActionValue& Value);
	void HandleCombatLeftClickReleased(const FInputActionValue& Value);
	void HandleCombatRightClickStarted(const FInputActionValue& Value);
	void HandleCombatRightClickReleased(const FInputActionValue& Value);
	void HandleCombatSwap(const FInputActionValue& Value);

	void SetCombatInputEnabled(bool bEnabled);
	void PressCombatAbility(const FGameplayTag& InputTag);
	void ReleaseCombatAbility(const FGameplayTag& InputTag);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual bool CanJumpInternal_Implementation() const override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

