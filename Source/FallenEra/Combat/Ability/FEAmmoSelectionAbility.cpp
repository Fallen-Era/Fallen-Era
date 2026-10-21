#include "Combat/Ability/FEAmmoSelectionAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/UI/FEAmmoRadialWidget.h"
#include "Combat/UI/FEAmmoSelectionViewModel.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"

UFE_AmmoSelectionAbility::UFE_AmmoSelectionAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	SetAssetTags(FGameplayTagContainer(FallenEraCombatGameplayTags::Ability_Combat_Reload));
	ActivationBlockedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_Reloading);
	ActivationBlockedTags.AddTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown);
}

void UFE_AmmoSelectionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bWheelOpened = false;
	bHoldElapsed = false;
	bOpeningWheel = false;
	bSelectingTagAdded = false;
	Equipment = ActorInfo->AvatarActor->FindComponentByClass<UFE_EquipmentComponent>();
	InputOwner = ActorInfo->PlayerController;
	APlayerController* Controller = InputOwner.Get();
	if (!Equipment.IsValid() || !Controller || !Controller->IsLocalController() || !UFE_CombatComponent::CanActorAttack(ActorInfo->AvatarActor.Get()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	WeaponInstanceId = Equipment->GetAmmoState().WeaponInstanceId;
	if (!WeaponInstanceId.IsValid()) { EndAbility(Handle, ActorInfo, ActivationInfo, false, true); return; }
	Equipment->OnAmmoChanged.AddUniqueDynamic(this, &ThisClass::HandleAmmoChanged);
	Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	if (FSlateApplication::IsInitialized())
	{
		ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::HandleApplicationActivation);
	}
	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	Release->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
	Release->ReadyForActivation();
	HoldTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.1f, Equipment->AmmoSelectionHoldDuration));
	HoldTask->OnFinish.AddDynamic(this, &ThisClass::OpenWheel);
	HoldTask->ReadyForActivation();
}

void UFE_AmmoSelectionAbility::OpenWheel()
{
	if (!IsActive() || bWheelOpened || bOpeningWheel) { return; }
	bHoldElapsed = true;
	APlayerController* Controller = InputOwner.Get();
	if (!Equipment.IsValid() || !Controller || !Controller->GetLocalPlayer() ||
		!UFE_CombatComponent::CanActorAttack(GetAvatarActorFromActorInfo()) ||
		GetAbilitySystemComponentFromActorInfo()->HasMatchingGameplayTag(FallenEraGameplayTags::State_InputBlocked)) { CancelSelection(); return; }
	UClass* WidgetClass = Equipment->GetAmmoSelectionWidgetClass();
	if (!WidgetClass)
	{
		if (Equipment->AmmoSelectionWidgetClass.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("Assign an AmmoSelectionWidgetClass WBP on %s"), *GetNameSafe(Equipment.Get()));
			CancelSelection();
			return;
		}
		if (!UIReadyHandle.IsValid()) { UIReadyHandle = Equipment->AmmoSelectionUIReady.AddUObject(this, &ThisClass::OpenWheel); }
		Equipment->PreloadAmmoSelectionUI(); // Async completion resumes this same hold, never activates another GA.
		return;
	}
	Equipment->AmmoSelectionUIReady.Remove(UIReadyHandle);
	UIReadyHandle.Reset();
	bOpeningWheel = true;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	bSelectingTagAdded = true;
	ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_SelectingAmmo);
	FGameplayTagContainer Actions(FallenEraCombatGameplayTags::Ability_Combat_Attack);
	Actions.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Aim);
	ASC->CancelAbilities(&Actions);
	if (!IsActive()) { return; } // A cancelled attack can synchronously change equipment/ammo state.
	Model = NewObject<UFE_AmmoSelectionViewModel>(this);
	Model->Initialize(Equipment.Get());
	Wheel = CreateWidget<UFE_AmmoRadialWidget>(Controller, WidgetClass);
	if (!Wheel || !Wheel->SetViewModel(Model)) { CancelSelection(); return; }
	Wheel->CancelRequested.BindUObject(this, &ThisClass::CancelSelection);
	if (!Wheel->AddToPlayerScreen(500)) { CancelSelection(); return; }
	if (!IsActive()) { return; }
	bWheelOpened = true;
	InputState.Capture(Controller, FSimpleDelegate::CreateUObject(this, &ThisClass::CancelSelection));
	if (!IsActive()) { return; }
	if (Controller->GetWorld()->GetGameViewport())
	{
		int32 Width = 0, Height = 0;
		Controller->GetViewportSize(Width, Height);
		const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
		Controller->SetMouseLocation(FMath::RoundToInt(Width * (LocalPlayer->Origin.X + LocalPlayer->Size.X * 0.5)),
			FMath::RoundToInt(Height * (LocalPlayer->Origin.Y + LocalPlayer->Size.Y * 0.5)));
	}
	bOpeningWheel = false;
	// No SetUserFocus/SetKeyboardFocus to the WBP: R must remain held in Enhanced Input.
}

void UFE_AmmoSelectionAbility::OnInputReleased(float TimeHeld)
{
	if (!IsActive()) { return; }
	TWeakObjectPtr<UFE_EquipmentComponent> Target = Equipment;
	const bool bConfirmedSelection = bHoldElapsed && Model && Model->ConfirmSelection();
	const FGuid AmmoId = bHoldElapsed ? bConfirmedSelection ? Model->GetConfirmedAmmoId() : FGuid() :
		Target.IsValid() ? Target->GetAmmoState().SelectedAmmoId : FGuid();
	const FGuid ExpectedWeapon = WeaponInstanceId;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	if (Target.IsValid() && AmmoId.IsValid()) { Target->RequestReloadAmmo(AmmoId, ExpectedWeapon); }
}

void UFE_AmmoSelectionAbility::HandleAmmoChanged()
{
	if (!Equipment.IsValid() || Equipment->GetAmmoState().WeaponInstanceId != WeaponInstanceId || Equipment->GetAmmoState().bReloading ||
		!UFE_CombatComponent::CanActorAttack(GetAvatarActorFromActorInfo())) { CancelSelection(); }
}

void UFE_AmmoSelectionAbility::HandleApplicationActivation(bool bActive)
{
	if (!bActive) { CancelSelection(); }
}

void UFE_AmmoSelectionAbility::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (NewPawn != GetAvatarActorFromActorInfo()) { CancelSelection(); }
}

void UFE_AmmoSelectionAbility::CancelSelection()
{
	if (IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true); }
}

void UFE_AmmoSelectionAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (HoldTask) { HoldTask->EndTask(); HoldTask = nullptr; }
	if (Equipment.IsValid())
	{
		Equipment->OnAmmoChanged.RemoveDynamic(this, &ThisClass::HandleAmmoChanged);
		Equipment->AmmoSelectionUIReady.Remove(UIReadyHandle);
	}
	UIReadyHandle.Reset();
	if (APlayerController* Controller = InputOwner.Get()) { Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged); }
	if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationHandle); }
	ActivationHandle.Reset();
	if (Wheel) { Wheel->CancelRequested.Unbind(); }
	InputState.Restore(); // Unconditional and idempotent; UI focus is not an ownership check.
	if (bSelectingTagAdded)
	{
		bSelectingTagAdded = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) { ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_SelectingAmmo); }
	}
	if (Wheel) { Wheel->RemoveFromParent(); Wheel = nullptr; }
	if (Model) { Model->Shutdown(); Model = nullptr; }
	bWheelOpened = false;
	bOpeningWheel = false;
	Equipment.Reset();
	InputOwner.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
