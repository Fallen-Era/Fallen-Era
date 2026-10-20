#include "Combat/Component/FEStaminaComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEPlayerMovementStatsComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/GameplayEffect/FEStaminaChangeGameplayEffect.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace { constexpr float StaminaUpdateInterval = 0.1f; }

UFE_StaminaComponent::UFE_StaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFE_StaminaComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystem();
}

void UFE_StaminaComponent::InitializeAbilitySystem()
{
	UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	if (AbilitySystem.Get() == ASC && (!ASC || StaminaHandle.IsValid())) { RefreshActivity(); return; }
	UnbindAbilitySystem();
	AbilitySystem = ASC;
	if (!ASC || !ASC->GetSet<UFallenEraAttributeSet>()) { return; }
	StaminaHandle = ASC->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
	MaxStaminaHandle = ASC->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetMaxStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleSettingsChanged);
	RecoveryRateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetStaminaRecoveryRateAttribute())
		.AddUObject(this, &ThisClass::HandleSettingsChanged);
	AimHandle = ASC->RegisterGameplayTagEvent(FallenEraCombatGameplayTags::State_Combat_Aiming)
		.AddUObject(this, &ThisClass::HandleStateTagChanged);
	DeathHandle = ASC->RegisterGameplayTagEvent(FallenEraGameplayTags::State_Dead)
		.AddUObject(this, &ThisClass::HandleStateTagChanged);
	ExhaustionHandle = ASC->RegisterGameplayTagEvent(FallenEraCombatGameplayTags::State_Stamina_Exhausted)
		.AddUObject(this, &ThisClass::HandleStateTagChanged);
	AbilityFailedHandle = ASC->AbilityFailedCallbacks.AddUObject(this, &ThisClass::HandleAbilityFailed);
	UpdateExhaustion();
	RefreshActivity();
}

void UFE_StaminaComponent::UnbindAbilitySystem()
{
	GetWorld()->GetTimerManager().ClearTimer(ResourceTimer);
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetStaminaAttribute()).Remove(StaminaHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetMaxStaminaAttribute()).Remove(MaxStaminaHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UFallenEraAttributeSet::GetStaminaRecoveryRateAttribute()).Remove(RecoveryRateHandle);
		ASC->RegisterGameplayTagEvent(FallenEraCombatGameplayTags::State_Combat_Aiming).Remove(AimHandle);
		ASC->RegisterGameplayTagEvent(FallenEraGameplayTags::State_Dead).Remove(DeathHandle);
		ASC->RegisterGameplayTagEvent(FallenEraCombatGameplayTags::State_Stamina_Exhausted).Remove(ExhaustionHandle);
		ASC->AbilityFailedCallbacks.Remove(AbilityFailedHandle);
	}
	AbilitySystem.Reset();
}

bool UFE_StaminaComponent::CanSpendStamina(float Amount) const
{
	if (!FMath::IsFinite(Amount) || Amount < 0.0f) { return false; }
	if (Amount == 0.0f) { return true; }
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	const UFallenEraAttributeSet* Stats = ASC ? ASC->GetSet<UFallenEraAttributeSet>() : nullptr;
	return Stats && !ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead) &&
		!ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Stamina_Exhausted) && Stats->GetStamina() >= Amount;
}

bool UFE_StaminaComponent::TrySpendStamina(float Amount)
{
	if (!CanSpendStamina(Amount))
	{
		if (FMath::IsFinite(Amount) && Amount > 0.0f) { NotifyActionRejected(); }
		return false;
	}
	if (GetOwner()->HasAuthority() && Amount > 0.0f) { ApplyStaminaChange(-Amount); }
	return true;
}

void UFE_StaminaComponent::HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags)
{
	if (FailureTags.HasTagExact(FallenEraCombatGameplayTags::Ability_ActivateFail_Stamina)) { NotifyActionRejected(); }
}

void UFE_StaminaComponent::NotifyActionRejected()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	// Local feedback only: no server broadcast, and no warnings for missing ASC/dead/blocked input.
	if (bFailureFeedbackSent || !Pawn || !Pawn->IsLocallyControlled() || !ASC || !ASC->GetSet<UFallenEraAttributeSet>() ||
		ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead) ||
		ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned) ||
		ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_InputBlocked)) { return; }
	bFailureFeedbackSent = true;
	OnActionRejected.Broadcast();
}

void UFE_StaminaComponent::ConsumeJumpStamina() { TrySpendStamina(JumpStaminaCost); }

bool UFE_StaminaComponent::CanSprintWithStamina() const
{
	return SprintStaminaPerSecond <= 0.0f || CanSpendStamina(UE_SMALL_NUMBER);
}

void UFE_StaminaComponent::SetSprintDraining(bool bEnabled)
{
	if (bSprintDraining == bEnabled) { return; }
	bSprintDraining = bEnabled;
	RefreshActivity();
}

bool UFE_StaminaComponent::IsRecoveryBlocked() const
{
	const UFE_WeaponAimingComponent* Aim = GetOwner()->FindComponentByClass<UFE_WeaponAimingComponent>();
	return (Aim && Aim->IsAiming()) || (AbilitySystem.IsValid() &&
		AbilitySystem->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_Aiming));
}

void UFE_StaminaComponent::RefreshActivity()
{
	if (!GetOwner()->HasAuthority()) { return; }
	GetWorld()->GetTimerManager().ClearTimer(ResourceTimer);
	LastUpdateTime = GetWorld()->GetTimeSeconds();
	UpdateTimer();
}

void UFE_StaminaComponent::HandleStaminaChanged(const FOnAttributeChangeData& Change)
{
	if (!GetOwner()->HasAuthority()) { return; }
	if (Change.NewValue < Change.OldValue)
	{
		RecoveryReadyTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, RecoveryDelay);
	}
	UpdateExhaustion();
	UpdateTimer();
}

void UFE_StaminaComponent::HandleSettingsChanged(const FOnAttributeChangeData& Change)
{
	UpdateExhaustion();
	UpdateTimer();
}

void UFE_StaminaComponent::HandleStateTagChanged(FGameplayTag Tag, int32 Count)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if ((GetOwner()->HasAuthority() || (Pawn && Pawn->IsLocallyControlled())) && Count > 0 &&
		(Tag == FallenEraCombatGameplayTags::State_Stamina_Exhausted || Tag == FallenEraGameplayTags::State_Dead))
	{
		if (UFE_PlayerMovementStatsComponent* Movement = GetOwner()->FindComponentByClass<UFE_PlayerMovementStatsComponent>())
		{
			Movement->SetSprinting(false);
		}
	}
	RefreshActivity();
}

void UFE_StaminaComponent::UpdateExhaustion()
{
	UAbilitySystemComponent* ASC = AbilitySystem.Get();
	const UFallenEraAttributeSet* Stats = ASC ? ASC->GetSet<UFallenEraAttributeSet>() : nullptr;
	if (!Stats || !GetOwner()->HasAuthority()) { return; }
	const bool bWasExhausted = ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Stamina_Exhausted);
	const float Threshold = FMath::Min(FMath::Max(0.0f, ExhaustionRecoveryThreshold), Stats->GetMaxStamina());
	const bool bExhausted = Stats->GetStamina() <= 0.0f || (bWasExhausted && Stats->GetStamina() < Threshold);
	if (bWasExhausted != bExhausted)
	{
		ASC->SetLooseGameplayTagCount(FallenEraCombatGameplayTags::State_Stamina_Exhausted, bExhausted ? 1 : 0,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void UFE_StaminaComponent::UpdateTimer()
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	const UFallenEraAttributeSet* Stats = ASC ? ASC->GetSet<UFallenEraAttributeSet>() : nullptr;
	if (!GetOwner()->HasAuthority()) { return; }
	const bool bAlive = Stats && !ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
	const bool bDrain = bAlive && bSprintDraining && SprintStaminaPerSecond > 0.0f && CanSprintWithStamina();
	const bool bRecover = bAlive && !bDrain && !IsRecoveryBlocked() &&
		Stats->GetStamina() < Stats->GetMaxStamina() && Stats->GetStaminaRecoveryRate() > 0.0f;
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (!bDrain && !bRecover) { Timers.ClearTimer(ResourceTimer); return; }
	if (!Timers.IsTimerActive(ResourceTimer))
	{
		LastUpdateTime = GetWorld()->GetTimeSeconds();
		const float FirstDelay = bDrain ? StaminaUpdateInterval :
			FMath::Max(StaminaUpdateInterval, float(RecoveryReadyTime - LastUpdateTime) + StaminaUpdateInterval);
		Timers.SetTimer(ResourceTimer, this, &ThisClass::UpdateResource, StaminaUpdateInterval, true, FirstDelay);
	}
}

void UFE_StaminaComponent::UpdateResource()
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	const UFallenEraAttributeSet* Stats = ASC ? ASC->GetSet<UFallenEraAttributeSet>() : nullptr;
	if (!Stats || !GetOwner()->HasAuthority() || ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead))
	{
		UpdateTimer();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float DeltaSeconds = float(FMath::Max(0.0, Now - LastUpdateTime));
	const float RecoverySeconds = float(FMath::Max(0.0, Now - FMath::Max(LastUpdateTime, RecoveryReadyTime)));
	LastUpdateTime = Now;
	if (bSprintDraining && SprintStaminaPerSecond > 0.0f && CanSprintWithStamina())
	{
		ApplyStaminaChange(-FMath::Min(Stats->GetStamina(), SprintStaminaPerSecond * DeltaSeconds));
	}
	else if (!IsRecoveryBlocked() && RecoverySeconds > 0.0f)
	{
		ApplyStaminaChange(FMath::Min(Stats->GetMaxStamina() - Stats->GetStamina(), Stats->GetStaminaRecoveryRate() * RecoverySeconds));
	}
	UpdateTimer();
}

void UFE_StaminaComponent::ApplyStaminaChange(float Delta)
{
	UAbilitySystemComponent* ASC = AbilitySystem.Get();
	if (!ASC || !FMath::IsFinite(Delta) || Delta == 0.0f) { return; }
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UFE_StaminaChangeGameplayEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_Stamina_Change, Delta);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UFE_StaminaComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	UnbindAbilitySystem();
	Super::EndPlay(Reason);
}
