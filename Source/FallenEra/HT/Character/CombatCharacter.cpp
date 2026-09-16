#include "HT/Character/CombatCharacter.h"

#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HT/Weapon/WeaponItemData.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

AFE_CombatCharacter::AFE_CombatCharacter()
{
	EquippedWorldWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedWorldWeaponMesh"));
	EquippedWorldWeaponMesh->SetupAttachment(GetMesh());
	EquippedWorldWeaponMesh->SetOwnerNoSee(true);
	EquippedWorldWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedWorldWeaponMesh->SetGenerateOverlapEvents(false);

	EquippedFirstPersonWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedFirstPersonWeaponMesh"));
	EquippedFirstPersonWeaponMesh->SetupAttachment(GetFirstPersonMesh());
	EquippedFirstPersonWeaponMesh->SetOnlyOwnerSee(true);
	EquippedFirstPersonWeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	EquippedFirstPersonWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedFirstPersonWeaponMesh->SetGenerateOverlapEvents(false);

	bReplicates = true;
}

void AFE_CombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetCombatInputEnabled(true);
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
	DOREPLIFETIME(AFE_CombatCharacter, CurrentItemTagIndex);
}

void AFE_CombatCharacter::CycleWeapon()
{
	if (!HasAuthority())
	{
		ServerCycleWeapon();
		return;
	}

	if (TestItemTags.IsEmpty())
	{
		return;
	}

	CurrentItemTagIndex = (CurrentItemTagIndex + 1 + TestItemTags.Num()) % TestItemTags.Num();
	EquipCurrentWeapon();
}

void AFE_CombatCharacter::ServerCycleWeapon_Implementation()
{
	CycleWeapon();
}

void AFE_CombatCharacter::EquipWeaponByItemTag(FGameplayTag ItemTag)
{
	if (!HasAuthority() || !ItemTag.IsValid())
	{
		return;
	}

	const int32 FoundIndex = TestItemTags.IndexOfByKey(ItemTag);
	if (FoundIndex == INDEX_NONE)
	{
		return;
	}

	CurrentItemTagIndex = FoundIndex;
	EquipCurrentWeapon();
}

FGameplayTag AFE_CombatCharacter::GetCurrentItemTag() const
{
	return TestItemTags.IsValidIndex(CurrentItemTagIndex) ? TestItemTags[CurrentItemTagIndex] : FGameplayTag();
}

const UFE_WeaponAttackData* AFE_CombatCharacter::GetCurrentAttackForInputTag(FGameplayTag InputTag) const
{
	return CurrentWeaponData ? CurrentWeaponData->FindAttackForInputTag(InputTag) : nullptr;
}

void AFE_CombatCharacter::OnRep_CurrentItemTagIndex()
{
	EquipCurrentWeapon();
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

void AFE_CombatCharacter::EquipCurrentWeapon()
{
	ClearWeaponAbilities();
	CurrentWeaponData = nullptr;

	const FGameplayTag CurrentTag = GetCurrentItemTag();
	if (CurrentTag.IsValid())
	{
		CurrentWeaponData = FindWeaponDataForTag(CurrentTag);
	}

	if (!CurrentWeaponData)
	{
		ClearWeaponVisuals();
		SetCombatInputEnabled(false);
		WeaponChangedDelegate.Broadcast(nullptr);
		return;
	}

	ApplyWeaponVisuals();

	CacheWeaponAbilities();
	SetCombatInputEnabled(true);
	WeaponChangedDelegate.Broadcast(CurrentWeaponData);
}

void AFE_CombatCharacter::ClearWeaponVisuals()
{
	if (EquippedWorldWeaponMesh)
	{
		EquippedWorldWeaponMesh->SetStaticMesh(nullptr);
	}
	if (EquippedFirstPersonWeaponMesh)
	{
		EquippedFirstPersonWeaponMesh->SetStaticMesh(nullptr);
	}

	UpdateWeaponAnimLayers(nullptr);
}

void AFE_CombatCharacter::ApplyWeaponVisuals()
{
	if (!CurrentWeaponData)
	{
		ClearWeaponVisuals();
		return;
	}

	UStaticMesh* WeaponMesh = CurrentWeaponData->Mesh.LoadSynchronous();
	if (EquippedWorldWeaponMesh)
	{
		EquippedWorldWeaponMesh->AttachToComponent(
			GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, CurrentWeaponData->AttachSocketName);
		EquippedWorldWeaponMesh->SetRelativeTransform(CurrentWeaponData->SocketOffset);
		EquippedWorldWeaponMesh->SetStaticMesh(WeaponMesh);
	}

	if (EquippedFirstPersonWeaponMesh)
	{
		EquippedFirstPersonWeaponMesh->AttachToComponent(
			GetFirstPersonMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, CurrentWeaponData->AttachSocketName);
		EquippedFirstPersonWeaponMesh->SetRelativeTransform(CurrentWeaponData->SocketOffset);
		EquippedFirstPersonWeaponMesh->SetStaticMesh(WeaponMesh);
	}

	UpdateWeaponAnimLayers(CurrentWeaponData->WeaponAnimLayerClass.LoadSynchronous());
}

void AFE_CombatCharacter::UpdateWeaponAnimLayers(TSubclassOf<UAnimInstance> NewAnimLayerClass)
{
	if (CurrentWeaponAnimLayerClass == NewAnimLayerClass)
	{
		return;
	}

	if (CurrentWeaponAnimLayerClass)
	{
		if (GetMesh())
		{
			GetMesh()->UnlinkAnimClassLayers(CurrentWeaponAnimLayerClass);
		}
		if (GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->UnlinkAnimClassLayers(CurrentWeaponAnimLayerClass);
		}
	}

	CurrentWeaponAnimLayerClass = NewAnimLayerClass;
	if (CurrentWeaponAnimLayerClass)
	{
		if (GetMesh())
		{
			GetMesh()->LinkAnimClassLayers(CurrentWeaponAnimLayerClass);
		}
		if (GetFirstPersonMesh())
		{
			GetFirstPersonMesh()->LinkAnimClassLayers(CurrentWeaponAnimLayerClass);
		}
	}
}

void AFE_CombatCharacter::CacheWeaponAbilities()
{
	if (!HasAuthority() || !CurrentWeaponData)
	{
		return;
	}

	UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent();
	if (!AbilitySystem)
	{
		return;
	}

	TSet<FGameplayTag> GrantedInputTags;
	for (const UFE_WeaponAttackData* AttackAction : CurrentWeaponData->AttackActions)
	{
		if (!AttackAction)
		{
			continue;
		}

		if (AttackAction->InputTag.IsValid() && GrantedInputTags.Contains(AttackAction->InputTag))
		{
			continue;
		}

		UClass* AbilityClass = AttackAction->AbilityClass.LoadSynchronous();
		if (!AbilityClass || !AbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, CurrentWeaponData);
		if (AttackAction->InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AttackAction->InputTag);
			GrantedInputTags.Add(AttackAction->InputTag);
		}
		GrantedWeaponAbilityHandles.Add(AbilitySystem->GiveAbility(AbilitySpec));
	}
}

void AFE_CombatCharacter::ClearWeaponAbilities()
{
	if (!HasAuthority())
	{
		GrantedWeaponAbilityHandles.Reset();
		return;
	}

	if (UFallenEraAbilitySystemComponent* AbilitySystem = GetFallenEraAbilitySystemComponent())
	{
		for (const FGameplayAbilitySpecHandle& Handle : GrantedWeaponAbilityHandles)
		{
			if (Handle.IsValid())
			{
				AbilitySystem->ClearAbility(Handle);
			}
		}
	}
	GrantedWeaponAbilityHandles.Reset();
}

UFE_WeaponItemData* AFE_CombatCharacter::FindWeaponDataForTag(FGameplayTag ItemTag) const
{
	for (const TSoftObjectPtr<UFE_WeaponItemData>& WeaponDataAsset : WeaponDataAssets)
	{
		UFE_WeaponItemData* WeaponData = WeaponDataAsset.LoadSynchronous();
		if (WeaponData && (WeaponData->HasItemTag(ItemTag) || WeaponData->WeaponTags.HasTagExact(ItemTag)))
		{
			return WeaponData;
		}
	}

	return nullptr;
}
