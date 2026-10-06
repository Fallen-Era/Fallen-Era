#include "Combat/Component/FEPlayerMovementStatsComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/GameplayEffect/FEMovementHandlingGameplayEffect.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UFE_PlayerMovementStatsComponent::UFE_PlayerMovementStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UFE_PlayerMovementStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->OnCharacterMovementUpdated.AddDynamic(
			this,
			&UFE_PlayerMovementStatsComponent::HandleCharacterMovementUpdated);
	}
	RefreshMovementState();
}

void UFE_PlayerMovementStatsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(
			this,
			&UFE_PlayerMovementStatsComponent::HandleCharacterMovementUpdated);
	}
	ClearMovementHandlingEffect();
	Super::EndPlay(EndPlayReason);
}

void UFE_PlayerMovementStatsComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFE_PlayerMovementStatsComponent, bSprinting);
}

void UFE_PlayerMovementStatsComponent::SetSprinting(bool bNewSprinting)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	SetSprintingInternal(bNewSprinting);
	if (!OwnerActor->HasAuthority())
	{
		ServerSetSprinting(bNewSprinting);
	}
}

void UFE_PlayerMovementStatsComponent::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	SetSprintingInternal(bNewSprinting);
}

void UFE_PlayerMovementStatsComponent::OnRep_Sprinting()
{
	ApplyMovementSpeed();
	EvaluateMovementState();
}

void UFE_PlayerMovementStatsComponent::SetSprintingInternal(bool bNewSprinting)
{
	if (bSprinting == bNewSprinting)
	{
		ApplyMovementSpeed();
		return;
	}

	bSprinting = bNewSprinting;
	ApplyMovementSpeed();
	EvaluateMovementState();
}

void UFE_PlayerMovementStatsComponent::ApplyMovementSpeed() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		Movement->MaxWalkSpeed = FMath::Max(0.0f, bSprinting ? SprintSpeed : WalkSpeed);
	}
}

void UFE_PlayerMovementStatsComponent::RefreshMovementState()
{
	ApplyMovementSpeed();
	// Force reapplication when ASC actor info was replaced while the movement state stayed unchanged.
	if (GetOwner() && GetOwner()->HasAuthority() && ActiveMovementState != EFE_PlayerMovementState::Idle &&
		!MovementHandlingEffectHandle.IsValid())
	{
		const EFE_PlayerMovementState StateToRestore = ActiveMovementState;
		ActiveMovementState = EFE_PlayerMovementState::Idle;
		ApplyMovementState(StateToRestore);
		return;
	}
	EvaluateMovementState();
}

void UFE_PlayerMovementStatsComponent::HandleCharacterMovementUpdated(
	float DeltaSeconds,
	FVector OldLocation,
	FVector OldVelocity)
{
	(void)DeltaSeconds;
	(void)OldLocation;
	(void)OldVelocity;
	EvaluateMovementState();
}

void UFE_PlayerMovementStatsComponent::EvaluateMovementState()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const bool bMovingOnGround = Movement && Movement->IsMovingOnGround() &&
		Movement->Velocity.SizeSquared2D() >= FMath::Square(FMath::Max(0.0f, MovingSpeedThreshold));
	const EFE_PlayerMovementState DesiredState = !bMovingOnGround
		? EFE_PlayerMovementState::Idle
		: (bSprinting ? EFE_PlayerMovementState::Sprinting : EFE_PlayerMovementState::Walking);

	if (DesiredState != ActiveMovementState)
	{
		ApplyMovementState(DesiredState);
	}
}

void UFE_PlayerMovementStatsComponent::ApplyMovementState(EFE_PlayerMovementState NewState)
{
	ActiveMovementState = NewState;
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = UFE_CombatComponent::FindAbilitySystemComponent(OwnerActor);
	if (!AbilitySystem)
	{
		return;
	}

	ClearMovementHandlingEffect();
	AbilitySystem->SetLooseGameplayTagCount(
		FallenEraCombatGameplayTags::State_Movement_Walking,
		NewState == EFE_PlayerMovementState::Walking ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);
	AbilitySystem->SetLooseGameplayTagCount(
		FallenEraCombatGameplayTags::State_Movement_Sprinting,
		NewState == EFE_PlayerMovementState::Sprinting ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);

	if (NewState == EFE_PlayerMovementState::Idle)
	{
		return;
	}

	const float AccuracyModifier = NewState == EFE_PlayerMovementState::Sprinting
		? SprintingAccuracyModifier
		: WalkingAccuracyModifier;
	const float RecoilModifier = NewState == EFE_PlayerMovementState::Sprinting
		? SprintingRecoilControlModifier
		: WalkingRecoilControlModifier;
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
		UFE_MovementHandlingGameplayEffect::StaticClass(),
		1.0f,
		AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_Accuracy, AccuracyModifier);
	Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_RecoilControl, RecoilModifier);
	MovementHandlingEffectHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UFE_PlayerMovementStatsComponent::ClearMovementHandlingEffect()
{
	if (MovementHandlingEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystem =
			UFE_CombatComponent::FindAbilitySystemComponent(GetOwner()))
		{
			AbilitySystem->RemoveActiveGameplayEffect(MovementHandlingEffectHandle);
		}
		MovementHandlingEffectHandle.Invalidate();
	}
}
