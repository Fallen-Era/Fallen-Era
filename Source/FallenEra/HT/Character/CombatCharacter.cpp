#include "HT/Character/CombatCharacter.h"

#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "HT/Component/CombatComponent.h"

AFE_CombatCharacter::AFE_CombatCharacter()
{
	EquipmentComponent = CreateDefaultSubobject<UFE_EquipmentComponent>(TEXT("EquipmentComponent"));
	bReplicates = true;
}

FFE_CombatDamageResult AFE_CombatCharacter::ReceiveCombatDamage_Implementation(
	const FFE_CombatDamageRequest& DamageRequest)
{
	UFE_CombatComponent* Combat = GetCombatComponent();
	return Combat
		? Combat->ApplyGameplayEffectDamage(DamageRequest)
		: FFE_CombatDamageResult();
}

void AFE_CombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetCombatInputEnabled(true);
}

void AFE_CombatCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// AFallenEraCharacter initializes the PlayerState ASC in Super. Re-evaluate
	// the equipped weapon afterward so its persistent Offense effect can bind to
	// the now-valid ASC even when the weapon was selected before possession.
	if (HasAuthority())
	{
		if (EquipmentComponent)
		{
			EquipmentComponent->RefreshEquipment();
		}
	}
}

void AFE_CombatCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (EquipmentComponent)
	{
		EquipmentComponent->RefreshEquipment();
	}
}

void AFE_CombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (CombatLeftClickAction)
		{
			EnhancedInputComponent->BindAction(CombatLeftClickAction, ETriggerEvent::Started, this, &AFE_CombatCharacter::HandleCombatLeftClickStarted);
			EnhancedInputComponent->BindAction(CombatLeftClickAction, ETriggerEvent::Completed, this, &AFE_CombatCharacter::HandleCombatLeftClickReleased);
			EnhancedInputComponent->BindAction(CombatLeftClickAction, ETriggerEvent::Canceled, this, &AFE_CombatCharacter::HandleCombatLeftClickReleased);
		}

		if (CombatRightClickAction)
		{
			EnhancedInputComponent->BindAction(CombatRightClickAction, ETriggerEvent::Started, this, &AFE_CombatCharacter::HandleCombatRightClickStarted);
			EnhancedInputComponent->BindAction(CombatRightClickAction, ETriggerEvent::Completed, this, &AFE_CombatCharacter::HandleCombatRightClickReleased);
			EnhancedInputComponent->BindAction(CombatRightClickAction, ETriggerEvent::Canceled, this, &AFE_CombatCharacter::HandleCombatRightClickReleased);
		}

		if (CombatSwapAction)
		{
			EnhancedInputComponent->BindAction(CombatSwapAction, ETriggerEvent::Started, this, &AFE_CombatCharacter::HandleCombatSwap);
		}
	}
}

void AFE_CombatCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AFE_CombatCharacter::PlayAttackMontage(UAnimMontage* Montage)
{
	if (UFE_CombatComponent* Combat = GetCombatComponent())
	{
		Combat->PlayAttackMontageLocal(Montage);
	}
}

void AFE_CombatCharacter::MulticastPlayAttackMontage_Implementation(UAnimMontage* Montage)
{
	if (!HasAuthority() && IsLocallyControlled())
	{
		return;
	}
	PlayAttackMontage(Montage);
}

void AFE_CombatCharacter::CycleWeapon()
{
	if (EquipmentComponent)
	{
		EquipmentComponent->CycleWeapon();
	}
}

void AFE_CombatCharacter::EquipWeaponByItemTag(FGameplayTag ItemTag)
{
	if (EquipmentComponent)
	{
		EquipmentComponent->EquipWeaponByItemTag(ItemTag);
	}
}

FGameplayTag AFE_CombatCharacter::GetCurrentItemTag() const
{
	return EquipmentComponent ? EquipmentComponent->GetCurrentItemTag() : FGameplayTag();
}

const UFE_WeaponItemData* AFE_CombatCharacter::GetCurrentWeaponData() const
{
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponData() : nullptr;
}

int32 AFE_CombatCharacter::GetCurrentItemTagIndex() const
{
	return EquipmentComponent ? EquipmentComponent->GetCurrentItemTagIndex() : INDEX_NONE;
}

const UFE_WeaponAttackData* AFE_CombatCharacter::GetCurrentAttackForInputTag(FGameplayTag InputTag) const
{
	return EquipmentComponent ? EquipmentComponent->GetCurrentAttackForInputTag(InputTag) : nullptr;
}

void AFE_CombatCharacter::HandleCombatLeftClickStarted(const FInputActionValue& Value)
{
	(void)Value;
	PressCombatAbility(FallenEraGameplayTags::Ability_Input_Combat_LeftClick);
}

void AFE_CombatCharacter::HandleCombatLeftClickReleased(const FInputActionValue& Value)
{
	(void)Value;
	ReleaseCombatAbility(FallenEraGameplayTags::Ability_Input_Combat_LeftClick);
}

void AFE_CombatCharacter::HandleCombatRightClickStarted(const FInputActionValue& Value)
{
	(void)Value;
	PressCombatAbility(FallenEraGameplayTags::Ability_Input_Combat_RightClick);
}

void AFE_CombatCharacter::HandleCombatRightClickReleased(const FInputActionValue& Value)
{
	(void)Value;
	ReleaseCombatAbility(FallenEraGameplayTags::Ability_Input_Combat_RightClick);
}

void AFE_CombatCharacter::HandleCombatSwap(const FInputActionValue& Value)
{
	if (Value.Get<bool>())
	{
		CycleWeapon();
	}
}

void AFE_CombatCharacter::PressCombatAbility(const FGameplayTag& InputTag)
{
	if (UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent())
	{
		AbilitySystem->AbilityInputTagPressed(InputTag);
	}
}

void AFE_CombatCharacter::ReleaseCombatAbility(const FGameplayTag& InputTag)
{
	if (UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent())
	{
		AbilitySystem->AbilityInputTagReleased(InputTag);
	}
}

void AFE_CombatCharacter::SetCombatInputEnabled(bool bEnabled)
{
	if (!IsLocallyControlled() || !CombatMappingContext)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer)
		: nullptr;
	if (!Subsystem)
	{
		return;
	}

	if (bEnabled)
	{
		Subsystem->AddMappingContext(CombatMappingContext, CombatMappingPriority);
	}
	else
	{
		Subsystem->RemoveMappingContext(CombatMappingContext);
	}
}
