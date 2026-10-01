// Copyright Epic Games, Inc. All Rights Reserved.

#include "FallenEraCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FallenEra.h"
#include "FallenEraPlayerState.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "HT/Combat/FECombatTeams.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Component/CharacterStatusComponent.h"
#include "HT/Component/EquipmentComponent.h"
#include "NavigationInvokerComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

AFallenEraCharacter::AFallenEraCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	CombatComponent = CreateDefaultSubobject<UFE_CombatComponent>(TEXT("CombatComponent"));
	EquipmentComponent = CreateDefaultSubobject<UFE_EquipmentComponent>(TEXT("EquipmentComponent"));
	CharacterStatusComponent = CreateDefaultSubobject<UFE_CharacterStatusComponent>(TEXT("CharacterStatusComponent"));
	PerceptionStimuliSourceComponent =
		CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuliSourceComponent"));
	NavigationInvokerComponent = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavigationInvokerComponent"));
	NavigationInvokerComponent->SetGenerationRadii(10000.0f, 12000.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Player"));
	bReplicates = true;
}

UAbilitySystemComponent* AFallenEraCharacter::GetAbilitySystemComponent() const
{
	return GetFallenEraAbilitySystemComponent();
}

UFallenEraAbilitySystemComponent* AFallenEraCharacter::GetFallenEraAbilitySystemComponent() const
{
	const AFallenEraPlayerState* FallenEraPlayerState = GetPlayerState<AFallenEraPlayerState>();
	return FallenEraPlayerState ? FallenEraPlayerState->GetFallenEraAbilitySystemComponent() : nullptr;
}

FGenericTeamId AFallenEraCharacter::GetGenericTeamId() const
{
	return FECombatTeams::Player;
}

FFE_CombatDamageResult AFallenEraCharacter::ReceiveCombatDamage_Implementation(
	const FFE_CombatDamageRequest& DamageRequest)
{
	return CombatComponent
		? CombatComponent->ApplyGameplayEffectDamage(DamageRequest)
		: FFE_CombatDamageResult();
}

void AFallenEraCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && PerceptionStimuliSourceComponent)
	{
		PerceptionStimuliSourceComponent->RegisterForSense(UAISense_Sight::StaticClass());
	}
	SetCombatInputEnabled(true);
}

void AFallenEraCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AFallenEraPlayerState* FallenEraPlayerState = GetPlayerState<AFallenEraPlayerState>())
	{
		FallenEraPlayerState->InitializeAbilitySystem(this);
	}
	if (HasAuthority() && EquipmentComponent)
	{
		EquipmentComponent->RefreshEquipment();
	}
}

void AFallenEraCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (AFallenEraPlayerState* FallenEraPlayerState = GetPlayerState<AFallenEraPlayerState>())
	{
		FallenEraPlayerState->InitializeAbilitySystem(this);
	}
	if (EquipmentComponent)
	{
		EquipmentComponent->RefreshEquipment();
	}
}

void AFallenEraCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AFallenEraCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFallenEraCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFallenEraCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFallenEraCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AFallenEraCharacter::LookInput);

		for (const FFallenEraAbilityInputBinding& Binding : AbilityInputBindings)
		{
			if (Binding.InputAction && Binding.InputTag.IsValid())
			{
				EnhancedInputComponent->BindAction(Binding.InputAction, ETriggerEvent::Started, this, &AFallenEraCharacter::AbilityInputPressed, Binding.InputTag);
				EnhancedInputComponent->BindAction(Binding.InputAction, ETriggerEvent::Completed, this, &AFallenEraCharacter::AbilityInputReleased, Binding.InputTag);
				EnhancedInputComponent->BindAction(Binding.InputAction, ETriggerEvent::Canceled, this, &AFallenEraCharacter::AbilityInputReleased, Binding.InputTag);
			}
		}

		if (CombatLeftClickAction)
		{
			EnhancedInputComponent->BindAction(
				CombatLeftClickAction, ETriggerEvent::Started,
				this, &AFallenEraCharacter::HandleCombatLeftClickStarted);
			EnhancedInputComponent->BindAction(
				CombatLeftClickAction, ETriggerEvent::Completed,
				this, &AFallenEraCharacter::HandleCombatLeftClickReleased);
			EnhancedInputComponent->BindAction(
				CombatLeftClickAction, ETriggerEvent::Canceled,
				this, &AFallenEraCharacter::HandleCombatLeftClickReleased);
		}

		if (CombatRightClickAction)
		{
			EnhancedInputComponent->BindAction(
				CombatRightClickAction, ETriggerEvent::Started,
				this, &AFallenEraCharacter::HandleCombatRightClickStarted);
			EnhancedInputComponent->BindAction(
				CombatRightClickAction, ETriggerEvent::Completed,
				this, &AFallenEraCharacter::HandleCombatRightClickReleased);
			EnhancedInputComponent->BindAction(
				CombatRightClickAction, ETriggerEvent::Canceled,
				this, &AFallenEraCharacter::HandleCombatRightClickReleased);
		}

		if (CombatSwapAction)
		{
			EnhancedInputComponent->BindAction(
				CombatSwapAction, ETriggerEvent::Started,
				this, &AFallenEraCharacter::HandleCombatSwap);
		}
	}
	else
	{
		UE_LOG(LogFallenEra, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AFallenEraCharacter::AbilityInputPressed(const FInputActionValue& Value, FGameplayTag InputTag)
{
	(void)Value;
	if (UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent())
	{
		AbilitySystem->AbilityInputTagPressed(InputTag);
	}
}

void AFallenEraCharacter::AbilityInputReleased(const FInputActionValue& Value, FGameplayTag InputTag)
{
	(void)Value;
	if (UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent())
	{
		AbilitySystem->AbilityInputTagReleased(InputTag);
	}
}


void AFallenEraCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AFallenEraCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AFallenEraCharacter::DoAim(float Yaw, float Pitch)
{
	const UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent();
	if (GetController() && (!AbilitySystem || !AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_InputBlocked)))
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AFallenEraCharacter::DoMove(float Right, float Forward)
{
	const UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent();
	const bool bMovementBlocked = AbilitySystem && (
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_InputBlocked) ||
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_MovementBlocked) ||
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned) ||
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead));
	if (GetController() && !bMovementBlocked)
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

bool AFallenEraCharacter::CanJumpInternal_Implementation() const
{
	const UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent();
	if (AbilitySystem && (
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_InputBlocked) ||
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_MovementBlocked) ||
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned) ||
		AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead)))
	{
		return false;
	}

	return Super::CanJumpInternal_Implementation();
}

void AFallenEraCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void AFallenEraCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void AFallenEraCharacter::PlayAttackMontage(UAnimMontage* Montage)
{
	if (CombatComponent)
	{
		CombatComponent->PlayAttackMontageLocal(Montage);
	}
}

void AFallenEraCharacter::MulticastPlayAttackMontage_Implementation(UAnimMontage* Montage)
{
	if (!HasAuthority() && IsLocallyControlled())
	{
		return;
	}
	PlayAttackMontage(Montage);
}

void AFallenEraCharacter::HandleCombatLeftClickStarted(const FInputActionValue& Value)
{
	(void)Value;
	PressCombatAbility(FallenEraGameplayTags::Ability_Input_Combat_LeftClick);
}

void AFallenEraCharacter::HandleCombatLeftClickReleased(const FInputActionValue& Value)
{
	(void)Value;
	ReleaseCombatAbility(FallenEraGameplayTags::Ability_Input_Combat_LeftClick);
}

void AFallenEraCharacter::HandleCombatRightClickStarted(const FInputActionValue& Value)
{
	(void)Value;
	PressCombatAbility(FallenEraGameplayTags::Ability_Input_Combat_RightClick);
}

void AFallenEraCharacter::HandleCombatRightClickReleased(const FInputActionValue& Value)
{
	(void)Value;
	ReleaseCombatAbility(FallenEraGameplayTags::Ability_Input_Combat_RightClick);
}

void AFallenEraCharacter::HandleCombatSwap(const FInputActionValue& Value)
{
	if (Value.Get<bool>() && EquipmentComponent)
	{
		EquipmentComponent->CycleWeapon();
	}
}

void AFallenEraCharacter::PressCombatAbility(const FGameplayTag& InputTag)
{
	if (UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent())
	{
		AbilitySystem->AbilityInputTagPressed(InputTag);
	}
}

void AFallenEraCharacter::ReleaseCombatAbility(const FGameplayTag& InputTag)
{
	if (UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent())
	{
		AbilitySystem->AbilityInputTagReleased(InputTag);
	}
}

void AFallenEraCharacter::SetCombatInputEnabled(bool bEnabled)
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
