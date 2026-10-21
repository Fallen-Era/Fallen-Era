#include "Combat/UI/FEPlayerStatusViewModel.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Component/FECharacterStatusComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEStaminaComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/FECombatGameplayTags.h"
#include "FallenEraPlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

namespace
{
	TArray<FGameplayAttribute> StatusAttributes()
	{
		return { UFallenEraAttributeSet::GetHealthAttribute(), UFallenEraAttributeSet::GetMaxHealthAttribute(),
			UFallenEraAttributeSet::GetStaminaAttribute(), UFallenEraAttributeSet::GetMaxStaminaAttribute() };
	}
}

void UFE_PlayerStatusViewModel::Initialize(APlayerController* PlayerController)
{
	if (Controller.Get() != PlayerController)
	{
		Shutdown();
		Controller = PlayerController;
		if (PlayerController)
		{
			PawnChangedHandle = PlayerController->GetOnNewPawnNotifier().AddUObject(this, &ThisClass::HandlePawnChanged);
			if (AFallenEraPlayerController* FEController = Cast<AFallenEraPlayerController>(PlayerController))
			{
				PlayerStateChangedHandle = FEController->OnPlayerStateReady().AddUObject(this, &ThisClass::RefreshSources);
			}
		}
	}
	RefreshSources();
}

void UFE_PlayerStatusViewModel::Shutdown()
{
	if (APlayerController* PC = Controller.Get())
	{
		PC->GetOnNewPawnNotifier().Remove(PawnChangedHandle);
		if (AFallenEraPlayerController* FEController = Cast<AFallenEraPlayerController>(PC))
		{
			FEController->OnPlayerStateReady().Remove(PlayerStateChangedHandle);
		}
	}
	Controller.Reset();
	UnbindSources();
}

void UFE_PlayerStatusViewModel::UnbindSources()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->RegisterGameplayTagEvent(FallenEraCombatGameplayTags::State_Stamina_Exhausted).Remove(StaminaStateHandle);
		const TArray<FGameplayAttribute> Attributes = StatusAttributes();
		for (int32 Index = 0; Index < AttributeChangedHandles.Num(); ++Index)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Attributes[Index]).Remove(AttributeChangedHandles[Index]);
		}
	}
	if (UFE_CharacterStatusComponent* Status = StatusComponent.Get())
	{
		Status->OnActiveConditionsChanged().Remove(ConditionsChangedHandle);
	}
	if (UFE_EquipmentComponent* Equipment = EquipmentComponent.Get())
	{
		Equipment->OnAmmoChanged.RemoveDynamic(this, &ThisClass::RefreshAmmo);
		Equipment->OnWeaponChanged().Remove(WeaponChangedHandle);
	}
	if (UFE_StaminaComponent* StaminaSource = StaminaComponent.Get()) { StaminaSource->OnActionRejected.Remove(StaminaRejectedHandle); }
	AttributeChangedHandles.Reset();
	StaminaStateHandle.Reset();
	ConditionsChangedHandle.Reset();
	AbilitySystem.Reset();
	StatusComponent.Reset();
	EquipmentComponent.Reset();
	WeaponChangedHandle.Reset();
	StaminaComponent.Reset();
	StaminaRejectedHandle.Reset();
}

void UFE_PlayerStatusViewModel::HandlePawnChanged(APawn* Pawn) { RefreshSources(); }

void UFE_PlayerStatusViewModel::RefreshSources()
{
	APlayerController* PC = Controller.Get();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UAbilitySystemComponent* ASC = PC ? UFE_CombatComponent::FindAbilitySystemComponent(PC->PlayerState) : nullptr;
	if (!ASC) { ASC = UFE_CombatComponent::FindAbilitySystemComponent(Pawn); }
	UFE_CharacterStatusComponent* Status = Pawn ? Pawn->FindComponentByClass<UFE_CharacterStatusComponent>() : nullptr;
	UFE_EquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UFE_EquipmentComponent>() : nullptr;
	UFE_StaminaComponent* StaminaSource = Pawn ? Pawn->FindComponentByClass<UFE_StaminaComponent>() : nullptr;
	if (AbilitySystem.Get() != ASC || StatusComponent.Get() != Status || EquipmentComponent.Get() != Equipment || StaminaComponent.Get() != StaminaSource)
	{
		UnbindSources();
		AbilitySystem = ASC;
		StatusComponent = Status;
		EquipmentComponent = Equipment;
		StaminaComponent = StaminaSource;
		if (StaminaSource)
		{
			StaminaRejectedHandle = StaminaSource->OnActionRejected.AddUObject(this, &ThisClass::HandleStaminaActionRejected);
		}
		if (ASC)
		{
			StaminaStateHandle = ASC->RegisterGameplayTagEvent(FallenEraCombatGameplayTags::State_Stamina_Exhausted)
				.AddUObject(this, &ThisClass::HandleStaminaStateChanged);
			for (const FGameplayAttribute& Attribute : StatusAttributes())
			{
				AttributeChangedHandles.Add(ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
					.AddUObject(this, &ThisClass::HandleAttributeChanged));
			}
		}
		if (Status)
		{
			ConditionsChangedHandle = Status->OnActiveConditionsChanged().AddUObject(this, &ThisClass::RefreshConditions);
		}
		if (Equipment)
		{
			Equipment->OnAmmoChanged.AddUniqueDynamic(this, &ThisClass::RefreshAmmo);
			WeaponChangedHandle = Equipment->OnWeaponChanged().AddUObject(this, &ThisClass::HandleWeaponChanged);
		}
	}
	RefreshValues();
	RefreshConditions();
	RefreshAmmo();
}

void UFE_PlayerStatusViewModel::HandleWeaponChanged(const UFE_WeaponItemData* Weapon)
{
	RefreshAmmo();
}

void UFE_PlayerStatusViewModel::RefreshAmmo()
{
	const UFE_EquipmentComponent* Equipment = EquipmentComponent.Get();
	const UFE_WeaponItemData* Weapon = Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
	const FFE_WeaponAmmoState Ammo = Equipment ? Equipment->GetAmmoState() : FFE_WeaponAmmoState();
	// Async weapon loading and owner-only ammo replication may complete in either order.
	const bool bShowAmmo = Weapon && Weapon->Template.AmmoSettings.bUsesAmmo &&
		Ammo.WeaponTag == Equipment->GetCurrentItemTag();
	UE_MVVM_SET_PROPERTY_VALUE(bUsesAmmo, bShowAmmo);
	UE_MVVM_SET_PROPERTY_VALUE(LoadedAmmo, bShowAmmo ? Ammo.LoadedAmmo : 0);
	UE_MVVM_SET_PROPERTY_VALUE(ReserveAmmo, bShowAmmo ? Ammo.ReserveAmmo : 0);
	AmmoChanged.Broadcast();
}

void UFE_PlayerStatusViewModel::HandleAttributeChanged(const FOnAttributeChangeData& Data) { RefreshValues(); }

void UFE_PlayerStatusViewModel::HandleStaminaStateChanged(FGameplayTag Tag, int32 Count) { RefreshValues(); }

void UFE_PlayerStatusViewModel::HandleStaminaActionRejected() { StaminaActionRejected.Broadcast(); }

void UFE_PlayerStatusViewModel::RefreshValues()
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	const UFallenEraAttributeSet* Attributes = ASC ? ASC->GetSet<UFallenEraAttributeSet>() : nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(Health, Attributes ? Attributes->GetHealth() : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Attributes ? Attributes->GetMaxHealth() : 1.0f);
	UE_MVVM_SET_PROPERTY_VALUE(Stamina, Attributes ? Attributes->GetStamina() : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, Attributes ? Attributes->GetMaxStamina() : 1.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bStaminaExhausted, ASC && ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Stamina_Exhausted));
	ValuesChanged.Broadcast();
}

void UFE_PlayerStatusViewModel::RefreshConditions()
{
	Conditions = StatusComponent.IsValid() ? StatusComponent->GetActiveConditionsView() : TArray<FFE_ActiveCharacterCondition>();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Conditions);
	ConditionsChanged.Broadcast();
}

bool UFE_PlayerStatusViewModel::GetConditionDefinition(FGameplayTag Tag, FFE_CharacterConditionDefinition& Definition) const
{
	return StatusComponent.IsValid() && StatusComponent->GetConditionDefinition(Tag, Definition);
}
