#include "Combat/Component/FEEquipmentComponent.h"

#include "Combat/FECombatGameplayTags.h"
#include "Combat/Ability/FEWeaponAimAbility.h"
#include "Combat/Ability/FEWeaponReloadAbility.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Interface/FECombatPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/GameplayEffect/FEWeaponOffenseGameplayEffect.h"
#include "Combat/Projectile/FECombatProjectile.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/UI/FECrossHairWidget.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraSystem.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UFE_EquipmentComponent::UFE_EquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UFE_EquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeAmmoInventory();
	if (bPreloadWeaponAssetsOnBeginPlay)
	{
		PreloadWeaponAssets();
	}
	RefreshEquipment();
}

void UFE_EquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFE_EquipmentComponent, CurrentItemTagIndex);
	DOREPLIFETIME(UFE_EquipmentComponent, WeaponAttachmentLoadout);
	DOREPLIFETIME(UFE_EquipmentComponent, EquippedArmor);
	DOREPLIFETIME(UFE_EquipmentComponent, bInitialArmorApplied);
	DOREPLIFETIME_CONDITION(UFE_EquipmentComponent, AmmoState, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFE_EquipmentComponent, AmmoInventory, COND_OwnerOnly);
}

void UFE_EquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bArmorEndingPlay = true;
	ClearWeaponAttachments();
	if (WarmWeaponLoadHandle) { WarmWeaponLoadHandle->CancelHandle(); WarmWeaponLoadHandle.Reset(); }
	if (ArmorLoadHandle)
	{
		ArmorLoadHandle->CancelHandle();
		ArmorLoadHandle.Reset();
	}
	if (WeaponLoadHandle)
	{
		WeaponLoadHandle->CancelHandle();
		WeaponLoadHandle.Reset();
	}
	if (WeaponPreloadHandle)
	{
		WeaponPreloadHandle->CancelHandle();
		WeaponPreloadHandle.Reset();
	}
	ClearArmorEffects();
	// PlayerState-owned ASCs may outlive the equipped character.
	ClearEquippedWeaponOffense();
	ClearWeaponAbilities();
	RestoreFirstPersonMeshTransform();
	Super::EndPlay(EndPlayReason);
}

void UFE_EquipmentComponent::RefreshEquipment()
{
	if (UFE_CombatComponent* Combat = GetOwner()->FindComponentByClass<UFE_CombatComponent>())
	{
		Combat->RefreshAbilitySystem();
	}
	EquipCurrentWeapon();
	RefreshArmorEquipment();
}

void UFE_EquipmentComponent::PreloadWeaponAssets()
{
	if (WeaponPreloadHandle)
	{
		WeaponPreloadHandle->CancelHandle();
		WeaponPreloadHandle.Reset();
	}

	TArray<FSoftObjectPath> WeaponDataPaths;
	for (const TSoftObjectPtr<UFE_WeaponItemData>& WeaponDataAsset : WeaponDataAssets)
	{
		if (!WeaponDataAsset.IsNull())
		{
			WeaponDataPaths.AddUnique(WeaponDataAsset.ToSoftObjectPath());
		}
	}
	if (WeaponDataPaths.IsEmpty())
	{
		return;
	}

	WeaponPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		WeaponDataPaths,
		FStreamableDelegate::CreateUObject(
			this, &UFE_EquipmentComponent::FinishPreloadingWeaponDataAssets));
}

void UFE_EquipmentComponent::CycleWeapon()
{
	if (!CanChangeEquipment() || TestItemTags.IsEmpty()) { return; }
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		InterruptWeaponActions();
		ServerCycleWeapon();
		return;
	}

	if (TestItemTags.IsEmpty())
	{
		return;
	}

	CurrentItemTagIndex = (CurrentItemTagIndex + 1 + TestItemTags.Num()) % TestItemTags.Num();
	LastEquipmentChangeTime = GetWorld()->GetTimeSeconds();
	EquipCurrentWeapon();
}

bool UFE_EquipmentComponent::CanChangeEquipment() const
{
	return !bArmorEndingPlay && GetWorld() && UFE_CombatComponent::CanActorAttack(GetOwner()) &&
		GetWorld()->GetTimeSeconds() - LastEquipmentChangeTime >= EquipmentChangeInterval;
}

bool UFE_EquipmentComponent::ServerCycleWeapon_Validate() { return true; }
bool UFE_EquipmentComponent::ServerUnequipWeapon_Validate() { return true; }

void UFE_EquipmentComponent::ServerCycleWeapon_Implementation()
{
	CycleWeapon();
}

void UFE_EquipmentComponent::EquipWeaponByItemTag(FGameplayTag ItemTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemTag.IsValid() || !CanChangeEquipment())
	{
		return;
	}

	const int32 FoundIndex = TestItemTags.IndexOfByKey(ItemTag);
	if (FoundIndex == INDEX_NONE || FoundIndex == CurrentItemTagIndex)
	{
		return;
	}

	CurrentItemTagIndex = FoundIndex;
	LastEquipmentChangeTime = GetWorld()->GetTimeSeconds();
	EquipCurrentWeapon();
}

void UFE_EquipmentComponent::UnequipWeapon()
{
	if (!CanChangeEquipment() || CurrentItemTagIndex == INDEX_NONE) { return; }
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		InterruptWeaponActions();
		ServerUnequipWeapon();
		return;
	}

	CurrentItemTagIndex = INDEX_NONE;
	LastEquipmentChangeTime = GetWorld()->GetTimeSeconds();
	EquipCurrentWeapon();
}

void UFE_EquipmentComponent::ServerUnequipWeapon_Implementation()
{
	UnequipWeapon();
}

FGameplayTag UFE_EquipmentComponent::GetCurrentItemTag() const
{
	return TestItemTags.IsValidIndex(CurrentItemTagIndex) ? TestItemTags[CurrentItemTagIndex] : FGameplayTag();
}

const UFE_WeaponAttackData* UFE_EquipmentComponent::GetCurrentAttackForInputTag(FGameplayTag InputTag) const
{
	return CurrentWeaponData ? CurrentWeaponData->FindAttackForInputTag(InputTag) : nullptr;
}

void UFE_EquipmentComponent::OnRep_CurrentItemTagIndex()
{
	EquipCurrentWeapon();
}

void UFE_EquipmentComponent::EquipCurrentWeapon()
{
	SaveLoadedWeaponAmmo();
	InterruptWeaponActions();
	CancelWeaponReload();
	AmmoInstanceItemTag = FGameplayTag();
	if (WeaponLoadHandle)
	{
		WeaponLoadHandle->CancelHandle();
		WeaponLoadHandle.Reset();
	}
	ClearEquippedWeaponOffense();
	ClearWeaponAbilities();
	CurrentWeaponData = nullptr;
	PublishAmmoState();
	ClearWeaponAttachments();
	PendingWeaponData = nullptr;
	ClearWeaponVisuals();

	const FGameplayTag CurrentTag = GetCurrentItemTag();
	if (!CurrentTag.IsValid())
	{
		CurrentWeaponInstance = FFE_WeaponInstanceData();
		WeaponInstanceTemplate = nullptr;
		WeaponChangedDelegate.Broadcast(nullptr);
		return;
	}

	PendingWeaponData = FindWeaponDataForTag(CurrentTag);
	TArray<FSoftObjectPath> UnloadedWeaponDataPaths;
	if (const auto* KnownAsset = WeaponAssetByTag.Find(CurrentTag))
	{
		if (!KnownAsset->IsValid()) { UnloadedWeaponDataPaths.Add(KnownAsset->ToSoftObjectPath()); }
	}
	else for (const TSoftObjectPtr<UFE_WeaponItemData>& WeaponDataAsset : WeaponDataAssets)
	{
		if (!WeaponDataAsset.IsNull() && !WeaponDataAsset.IsValid())
		{
			UnloadedWeaponDataPaths.AddUnique(WeaponDataAsset.ToSoftObjectPath());
		}
	}
	if (!PendingWeaponData && !UnloadedWeaponDataPaths.IsEmpty())
	{
		WeaponChangedDelegate.Broadcast(nullptr);
		RequestWeaponLoad(UnloadedWeaponDataPaths, true);
		return;
	}

	PendingWeaponData = FindWeaponDataForTag(CurrentTag);
	if (!PendingWeaponData)
	{
		WeaponChangedDelegate.Broadcast(nullptr);
		return;
	}

	TArray<FSoftObjectPath> DependencyPaths;
	GatherWeaponDependencyPaths(PendingWeaponData, DependencyPaths);
	if (!DependencyPaths.IsEmpty())
	{
		WeaponChangedDelegate.Broadcast(nullptr);
		RequestWeaponLoad(DependencyPaths, false);
		return;
	}

	ApplyLoadedWeapon(PendingWeaponData);
}

void UFE_EquipmentComponent::InterruptWeaponActions()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = UFE_CombatComponent::FindAbilitySystemComponent(OwnerActor))
	{
		FGameplayTagContainer AttackAbilityTags;
		AttackAbilityTags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Attack);
		AttackAbilityTags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Aim);
		AttackAbilityTags.AddTag(FallenEraCombatGameplayTags::Ability_Combat_Reload);
		AbilitySystem->CancelAbilities(&AttackAbilityTags);
		if (UFallenEraAbilitySystemComponent* FallenEraAbilitySystem =
			Cast<UFallenEraAbilitySystemComponent>(AbilitySystem))
		{
			FallenEraAbilitySystem->ClearAbilityInput();
		}
	}

	if (UFE_CombatComponent* Combat = OwnerActor->FindComponentByClass<UFE_CombatComponent>())
	{
		Combat->StopWeaponActionPresentation(true);
	}
}

void UFE_EquipmentComponent::FinishLoadingWeaponDataAssets()
{
	WeaponLoadHandle.Reset();
	if (!bArmorEndingPlay)
	{
		EquipCurrentWeapon();
	}
}

void UFE_EquipmentComponent::FinishLoadingWeaponDependencies()
{
	UFE_WeaponItemData* LoadedWeaponData = PendingWeaponData;
	if (bArmorEndingPlay)
	{
		return;
	}
	if (!LoadedWeaponData ||
		!(LoadedWeaponData->HasItemTag(GetCurrentItemTag()) || LoadedWeaponData->Template.WeaponTags.HasTagExact(GetCurrentItemTag())))
	{
		EquipCurrentWeapon();
		return;
	}
	ApplyLoadedWeapon(LoadedWeaponData);
}

void UFE_EquipmentComponent::FinishPreloadingWeaponDataAssets()
{
	if (bArmorEndingPlay)
	{
		return;
	}

	// Cache paths, not UObject pointers: catalog montages can be collected once this initial scan finishes.
	for (const auto& Tag : TestItemTags)
	{
		if (!WeaponAssetByTag.Contains(Tag))
		{
			if (UFE_WeaponItemData* Data = FindWeaponDataForTag(Tag)) { WeaponAssetByTag.Add(Tag, Data); }
		}
	}
	TArray<FSoftObjectPath> PreloadPaths;
	if (!TestItemTags.IsEmpty())
	{
		for (int32 Offset = 0; Offset < 2; ++Offset)
		{
			const int32 Index = (FMath::Max(0, CurrentItemTagIndex) + Offset) % TestItemTags.Num();
			if (const auto* Asset = WeaponAssetByTag.Find(TestItemTags[Index]))
			{
				PreloadPaths.AddUnique(Asset->ToSoftObjectPath());
			}
		}
	}
	if (WarmWeaponLoadHandle) { WarmWeaponLoadHandle->CancelHandle(); }
	WarmWeaponLoadHandle = PreloadPaths.IsEmpty()
		? nullptr
		: UAssetManager::GetStreamableManager().RequestAsyncLoad(PreloadPaths,
			FStreamableDelegate::CreateUObject(this, &ThisClass::FinishWarmingWeaponDataAssets));
	WeaponPreloadHandle.Reset();
}

void UFE_EquipmentComponent::FinishWarmingWeaponDataAssets()
{
	if (bArmorEndingPlay || TestItemTags.IsEmpty()) { return; }
	TArray<FSoftObjectPath> Paths;
	for (int32 Offset = 0; Offset < 2; ++Offset)
	{
		const int32 Index = (FMath::Max(0, CurrentItemTagIndex) + Offset) % TestItemTags.Num();
		if (const auto* Asset = WeaponAssetByTag.Find(TestItemTags[Index]))
		{
			Paths.AddUnique(Asset->ToSoftObjectPath());
			GatherWeaponDependencyPaths(Asset->Get(), Paths, true);
		}
	}
	WarmWeaponLoadHandle = Paths.IsEmpty() ? nullptr : UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
}

void UFE_EquipmentComponent::ApplyLoadedWeapon(UFE_WeaponItemData* WeaponData)
{
	if (WeaponInstanceTemplate.Get() != WeaponData)
	{
		CurrentWeaponInstance = WeaponData->Instance;
		if (!CurrentWeaponInstance.InstanceId.IsValid()) { CurrentWeaponInstance.InstanceId = FGuid::NewGuid(); }
		WeaponInstanceTemplate = WeaponData;
		if (GetOwner()->HasAuthority()) { WeaponAttachmentLoadout.WeaponTag = FGameplayTag(); }
	}
	if (!CurrentWeaponInstance.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid equipment instance seed on %s"), *GetNameSafe(WeaponData));
		WeaponChangedDelegate.Broadcast(nullptr);
		return;
	}
	CurrentWeaponData = WeaponData;
	PendingWeaponData = nullptr;

	ApplyEquippedWeaponOffense();
	ApplyWeaponVisuals();
	InitializeWeaponAttachments();
	CacheWeaponAbilities();
	InitializeWeaponAmmo();
	if (GetOwner()->HasAuthority() && CurrentWeaponInstance.LoadedAmmo == 0 && CanReloadWeapon())
	{
		const bool bChargedOnly = !WeaponData->Template.AttackActions.IsEmpty() &&
			WeaponData->Template.AttackActions.ContainsByPredicate([](const TObjectPtr<UFE_WeaponAttackData>& Attack) { return Attack && Attack->IsA<UFE_ChargedProjectileAttackData>(); });
		if (bChargedOnly) { CompleteAmmoAttack(WeaponData, true, true); }
		else { RequestReload(); }
	}
	WeaponChangedDelegate.Broadcast(CurrentWeaponData);
	if (bPreloadWeaponAssetsOnBeginPlay) { FinishPreloadingWeaponDataAssets(); }
}

bool UFE_EquipmentComponent::SetWeaponInstanceData(const FFE_WeaponInstanceData& NewInstance)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrentWeaponData || !NewInstance.IsValid()) { return false; }
	if (CurrentWeaponData->Template.AmmoSettings.bUsesAmmo && NewInstance.LoadedAmmo > CurrentWeaponData->Template.AmmoSettings.MagazineCapacity) { return false; }
	InterruptWeaponActions();
	CurrentWeaponInstance = NewInstance;
	if (!CurrentWeaponInstance.InstanceId.IsValid()) { CurrentWeaponInstance.InstanceId = FGuid::NewGuid(); }
	WeaponInstanceTemplate = CurrentWeaponData;
	ClearEquippedWeaponOffense();
	ApplyEquippedWeaponOffense();
	WeaponAttachmentLoadout.WeaponTag = FGameplayTag();
	InitializeWeaponAttachments();
	SaveLoadedWeaponAmmo();
	PublishAmmoState();
	return true;
}

void UFE_EquipmentComponent::RequestWeaponLoad(
	const TArray<FSoftObjectPath>& AssetPaths,
	bool bLoadingDataAssets)
{
	if (AssetPaths.IsEmpty())
	{
		return;
	}
	const FStreamableDelegate Completion = bLoadingDataAssets
		? FStreamableDelegate::CreateUObject(this, &UFE_EquipmentComponent::FinishLoadingWeaponDataAssets)
		: FStreamableDelegate::CreateUObject(this, &UFE_EquipmentComponent::FinishLoadingWeaponDependencies);
	WeaponLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(AssetPaths, Completion);
}

void UFE_EquipmentComponent::GatherWeaponDependencyPaths(
	const UFE_WeaponItemData* WeaponData,
	TArray<FSoftObjectPath>& OutPaths,
	bool bIncludeLoadedAssets) const
{
	if (!WeaponData)
	{
		return;
	}
	auto AddPath = [&OutPaths, bIncludeLoadedAssets](const FSoftObjectPath& Path, bool bIsLoaded)
	{
		if (Path.IsValid() && (bIncludeLoadedAssets || !bIsLoaded))
		{
			OutPaths.AddUnique(Path);
		}
	};
	if (!WeaponData->Template.SkeletalMesh.IsNull())
	{
		AddPath(WeaponData->Template.SkeletalMesh.ToSoftObjectPath(), WeaponData->Template.SkeletalMesh.IsValid());
	}
	else
	{
		AddPath(WeaponData->Template.Mesh.ToSoftObjectPath(), WeaponData->Template.Mesh.IsValid());
	}
	AddPath(WeaponData->Template.WeaponAnimLayerClass.ToSoftObjectPath(), WeaponData->Template.WeaponAnimLayerClass.IsValid());
	AddPath(WeaponData->Template.WeaponMeshAnimInstanceClass.ToSoftObjectPath(), WeaponData->Template.WeaponMeshAnimInstanceClass.IsValid());
	AddPath(WeaponData->Template.CrosshairWidgetClass.ToSoftObjectPath(), WeaponData->Template.CrosshairWidgetClass.IsValid());
	AddPath(WeaponData->Template.AttackSound.ToSoftObjectPath(), WeaponData->Template.AttackSound.IsValid());
	AddPath(WeaponData->Template.DefaultDamageEffect.ToSoftObjectPath(), WeaponData->Template.DefaultDamageEffect.IsValid());
	AddPath(WeaponData->Template.AmmoSettings.ReloadMontage.ToSoftObjectPath(), WeaponData->Template.AmmoSettings.ReloadMontage.IsValid());
	AddPath(WeaponData->Template.AmmoSettings.WeaponReloadMontage.ToSoftObjectPath(), WeaponData->Template.AmmoSettings.WeaponReloadMontage.IsValid());
	if (const UFE_RangedWeaponItemData* RangedWeaponData = Cast<UFE_RangedWeaponItemData>(WeaponData))
	{
		AddPath(
			RangedWeaponData->Template.MuzzleNiagaraSystem.ToSoftObjectPath(),
			RangedWeaponData->Template.MuzzleNiagaraSystem.IsValid());
		AddPath(
			RangedWeaponData->Template.MuzzleParticleSystem.ToSoftObjectPath(),
			RangedWeaponData->Template.MuzzleParticleSystem.IsValid());
	}
	for (const UFE_WeaponAttackData* AttackData : WeaponData->Template.AttackActions)
	{
		if (!AttackData)
		{
			continue;
		}
		AddPath(AttackData->AbilityClass.ToSoftObjectPath(), AttackData->AbilityClass.IsValid());
		AddPath(AttackData->DamageEffectOverride.ToSoftObjectPath(), AttackData->DamageEffectOverride.IsValid());
		if (const UFE_ProjectileAttackDataBase* ProjectileData = Cast<UFE_ProjectileAttackDataBase>(AttackData))
		{
			AddPath(ProjectileData->ProjectileClass.ToSoftObjectPath(), ProjectileData->ProjectileClass.IsValid());
		}
		if (const UFE_GrenadeAttackData* GrenadeData = Cast<UFE_GrenadeAttackData>(AttackData))
		{
			AddPath(
				GrenadeData->TrajectoryNiagaraSystem.ToSoftObjectPath(),
				GrenadeData->TrajectoryNiagaraSystem.IsValid());
		}
	}
}

void UFE_EquipmentComponent::ApplyEquippedWeaponOffense()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrentWeaponData)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	if (!AbilitySystem)
	{
		return;
	}
	WeaponAbilitySystem = AbilitySystem;

	const FFE_WeaponStat& WeaponStats = CurrentWeaponInstance.ResolveStats();
	const float WeaponOffense = FMath::Max(WeaponStats.Offense, 0.0f);

	const FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	const FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
		UFE_WeaponOffenseGameplayEffect::StaticClass(), 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		return;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_AttackPower, WeaponOffense);
	// Attribute defaults are 1. Add only the equipped weapon's deviation so later
	// attachment/aim/trait GameplayEffects can continue to stack additively.
	EffectSpec.Data->SetSetByCallerMagnitude(
		FallenEraCombatGameplayTags::SetByCaller_Accuracy,
		FMath::Clamp(WeaponStats.Accuracy, 0.0f, 1.0f) - 1.0f);
	EffectSpec.Data->SetSetByCallerMagnitude(
		FallenEraCombatGameplayTags::SetByCaller_RecoilControl,
		FMath::Clamp(WeaponStats.RecoilControl, 0.0f, 1.0f) - 1.0f);
	EquippedWeaponOffenseEffectHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
}

void UFE_EquipmentComponent::ClearEquippedWeaponOffense()
{
	if (EquippedWeaponOffenseEffectHandle.IsValid() && GetOwner() && GetOwner()->HasAuthority())
	{
		if (UAbilitySystemComponent* AbilitySystem = WeaponAbilitySystem.Get())
		{
			AbilitySystem->RemoveActiveGameplayEffect(EquippedWeaponOffenseEffectHandle);
		}
	}

	EquippedWeaponOffenseEffectHandle = FActiveGameplayEffectHandle();
}

void UFE_EquipmentComponent::ClearWeaponVisuals()
{
	RestoreFirstPersonMeshTransform();
	auto ClearMesh = [](UMeshComponent* MeshComponent)
	{
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
		{
			StaticMeshComponent->SetStaticMesh(nullptr);
		}
		else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent))
		{
			SkeletalMeshComponent->SetAnimInstanceClass(nullptr);
			SkeletalMeshComponent->SetSkeletalMeshAsset(nullptr);
		}
	};
	ClearMesh(EquippedWorldWeaponMesh);
	ClearMesh(EquippedFirstPersonWeaponMesh);
	UpdateWeaponAnimLayers(nullptr);
}

void UFE_EquipmentComponent::RestoreFirstPersonMeshTransform()
{
	if (DefaultFirstPersonMesh.IsValid())
	{
		DefaultFirstPersonMesh->SetRelativeTransform(UnequippedFirstPersonMeshTransform);
	}
}

void UFE_EquipmentComponent::EnsureWeaponMeshComponents(bool bUseSkeletalMesh)
{
	ACharacter* CombatCharacter = Cast<ACharacter>(GetOwner());
	if (!CombatCharacter || CombatCharacter->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	auto IsExpectedType = [bUseSkeletalMesh](const UMeshComponent* MeshComponent)
	{
		return MeshComponent && (bUseSkeletalMesh
			? MeshComponent->IsA<USkeletalMeshComponent>()
			: MeshComponent->IsA<UStaticMeshComponent>());
	};
	auto DestroyWrongType = [&IsExpectedType](TObjectPtr<UMeshComponent>& MeshComponent)
	{
		if (MeshComponent && !IsExpectedType(MeshComponent))
		{
			MeshComponent->DestroyComponent();
			MeshComponent = nullptr;
		}
	};
	DestroyWrongType(EquippedWorldWeaponMesh);
	DestroyWrongType(EquippedFirstPersonWeaponMesh);
	const bool bHasFirstPersonRepresentation =
		IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter) != nullptr;

	auto CreateMeshComponent = [CombatCharacter, bUseSkeletalMesh](
		const TCHAR* BaseName, USceneComponent* AttachParent) -> UMeshComponent*
	{
		if (!AttachParent)
		{
			return nullptr;
		}

		UMeshComponent* NewMeshComponent = bUseSkeletalMesh
			? static_cast<UMeshComponent*>(NewObject<USkeletalMeshComponent>(CombatCharacter, BaseName))
			: static_cast<UMeshComponent*>(NewObject<UStaticMeshComponent>(CombatCharacter, BaseName));
		NewMeshComponent->CreationMethod = EComponentCreationMethod::Instance;
		CombatCharacter->AddInstanceComponent(NewMeshComponent);
		NewMeshComponent->SetupAttachment(AttachParent);
		NewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NewMeshComponent->SetGenerateOverlapEvents(false);
		NewMeshComponent->RegisterComponent();
		return NewMeshComponent;
	};

	if (!EquippedWorldWeaponMesh)
	{
		EquippedWorldWeaponMesh = CreateMeshComponent(
			bUseSkeletalMesh ? TEXT("EquippedWorldWeaponSkeletalMesh") : TEXT("EquippedWorldWeaponStaticMesh"),
			CombatCharacter->GetMesh());
	}
	if (!EquippedFirstPersonWeaponMesh)
	{
		EquippedFirstPersonWeaponMesh = CreateMeshComponent(
			bUseSkeletalMesh ? TEXT("EquippedFirstPersonWeaponSkeletalMesh") : TEXT("EquippedFirstPersonWeaponStaticMesh"),
			IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter));
	}

	// Mirror the character's first-person rendering setup. The local player sees the
	// first-person weapon, while its world-space representation supplies the VSM shadow.
	if (EquippedWorldWeaponMesh)
	{
		EquippedWorldWeaponMesh->SetCastShadow(true);
		EquippedWorldWeaponMesh->SetOnlyOwnerSee(false);
		EquippedWorldWeaponMesh->SetOwnerNoSee(bHasFirstPersonRepresentation);
		EquippedWorldWeaponMesh->SetCastHiddenShadow(false);
		EquippedWorldWeaponMesh->FirstPersonPrimitiveType = bHasFirstPersonRepresentation
			? EFirstPersonPrimitiveType::WorldSpaceRepresentation
			: EFirstPersonPrimitiveType::None;
		EquippedWorldWeaponMesh->MarkRenderStateDirty();
	}
	if (EquippedFirstPersonWeaponMesh)
	{
		EquippedFirstPersonWeaponMesh->SetCastShadow(true);
		EquippedFirstPersonWeaponMesh->SetOwnerNoSee(false);
		EquippedFirstPersonWeaponMesh->SetOnlyOwnerSee(true);
		EquippedFirstPersonWeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		EquippedFirstPersonWeaponMesh->MarkRenderStateDirty();
	}
	RefreshPerspectivePresentation();
}

void UFE_EquipmentComponent::RefreshPerspectivePresentation()
{
	const bool bFirstPerson = IFE_CombatPresentation::UsesFirstPersonMeshes(GetOwner());
	const bool bHasFirstPersonMesh = IFE_CombatPresentation::FindFirstPersonMesh(GetOwner()) != nullptr;
	const auto UpdateBranch = [bFirstPerson, bHasFirstPersonMesh](UMeshComponent* Root, bool bFirstPersonBranch)
	{
		if (!Root) { return; }
		TArray<USceneComponent*> Components;
		Root->GetChildrenComponents(true, Components);
		Components.Add(Root);
		for (USceneComponent* Component : Components)
		{
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Primitive->SetFirstPersonPrimitiveType(bFirstPersonBranch ? EFirstPersonPrimitiveType::FirstPerson :
					(bHasFirstPersonMesh && bFirstPerson ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None));
				Primitive->SetOnlyOwnerSee(bFirstPersonBranch);
				Primitive->SetOwnerNoSee(bFirstPersonBranch ? !bFirstPerson : (bHasFirstPersonMesh && bFirstPerson));
			}
		}
	};
	UpdateBranch(EquippedWorldWeaponMesh, false);
	UpdateBranch(EquippedFirstPersonWeaponMesh, true);
}

void UFE_EquipmentComponent::ApplyWeaponVisuals()
{
	if (!CurrentWeaponData)
	{
		ClearWeaponVisuals();
		return;
	}

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetOwner());
	const bool bUseSkeletalMesh = !CurrentWeaponData->Template.SkeletalMesh.IsNull();
	EnsureWeaponMeshComponents(bUseSkeletalMesh);
	if (USkeletalMeshComponent* Arms = IFE_CombatPresentation::FindFirstPersonMesh(GetOwner()))
	{
		// Capture once per component, never from an equipped/aiming pose on a later swap.
		if (DefaultFirstPersonMesh.Get() != Arms)
		{
			RestoreFirstPersonMeshTransform();
			DefaultFirstPersonMesh = Arms;
			UnequippedFirstPersonMeshTransform = Arms->GetRelativeTransform();
		}
		Arms->SetRelativeTransform(CurrentWeaponData->Template.FirstPersonMeshTransform);
	}

	auto ApplyMeshAsset = [this, bUseSkeletalMesh](UMeshComponent* MeshComponent)
	{
		if (bUseSkeletalMesh)
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent))
			{
				SkeletalMeshComponent->SetSkeletalMeshAsset(CurrentWeaponData->Template.SkeletalMesh.Get());
				SkeletalMeshComponent->SetAnimInstanceClass(CurrentWeaponData->Template.WeaponMeshAnimInstanceClass.Get());
			}
		}
		else if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
		{
			StaticMeshComponent->SetStaticMesh(CurrentWeaponData->Template.Mesh.Get());
		}
	};
	if (EquippedWorldWeaponMesh && CombatCharacter)
	{
		EquippedWorldWeaponMesh->AttachToComponent(
			CombatCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			CurrentWeaponData->Template.AttachSocketName);
		EquippedWorldWeaponMesh->SetRelativeTransform(CurrentWeaponData->Template.SocketOffset);
		ApplyMeshAsset(EquippedWorldWeaponMesh);
	}

	if (EquippedFirstPersonWeaponMesh && CombatCharacter && IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter))
	{
		EquippedFirstPersonWeaponMesh->AttachToComponent(
			IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter), FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			CurrentWeaponData->Template.AttachSocketName);
		EquippedFirstPersonWeaponMesh->SetRelativeTransform(CurrentWeaponData->Template.SocketOffset);
		ApplyMeshAsset(EquippedFirstPersonWeaponMesh);
	}

	UpdateWeaponAnimLayers(CurrentWeaponData->Template.WeaponAnimLayerClass.Get());
}

void UFE_EquipmentComponent::UpdateWeaponAnimLayers(TSubclassOf<UAnimInstance> NewAnimLayerClass)
{
	const ACharacter* CombatCharacter = Cast<ACharacter>(GetOwner());
	if (!CombatCharacter || CurrentWeaponAnimLayerClass == NewAnimLayerClass)
	{
		return;
	}

	if (CurrentWeaponAnimLayerClass)
	{
		if (CombatCharacter->GetMesh())
		{
			CombatCharacter->GetMesh()->UnlinkAnimClassLayers(CurrentWeaponAnimLayerClass);
		}
		if (IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter))
		{
			IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter)->UnlinkAnimClassLayers(CurrentWeaponAnimLayerClass);
		}
	}

	CurrentWeaponAnimLayerClass = NewAnimLayerClass;
	if (CurrentWeaponAnimLayerClass)
	{
		if (CombatCharacter->GetMesh())
		{
			CombatCharacter->GetMesh()->LinkAnimClassLayers(CurrentWeaponAnimLayerClass);
		}
		if (IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter))
		{
			IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter)->LinkAnimClassLayers(CurrentWeaponAnimLayerClass);
		}
	}
}

void UFE_EquipmentComponent::CacheWeaponAbilities()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrentWeaponData)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	if (!AbilitySystem)
	{
		return;
	}
	WeaponAbilitySystem = AbilitySystem;

	TSet<FGameplayTag> GrantedInputTags;
	if (CurrentWeaponData->Template.AmmoSettings.bUsesAmmo)
	{
		FGameplayAbilitySpec ReloadSpec(UFE_WeaponReloadAbility::StaticClass(), 1, INDEX_NONE, CurrentWeaponData);
		ReloadSpec.GetDynamicSpecSourceTags().AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_Reload);
		GrantedWeaponAbilityHandles.Add(AbilitySystem->GiveAbility(ReloadSpec));
		GrantedInputTags.Add(FallenEraCombatGameplayTags::Ability_Input_Combat_Reload);
	}
	if (const UFE_RangedWeaponItemData* Ranged = Cast<UFE_RangedWeaponItemData>(CurrentWeaponData);
		Ranged && Ranged->Template.bSupportsAiming)
	{
		FGameplayAbilitySpec AimSpec(UFE_WeaponAimAbility::StaticClass(), 1, INDEX_NONE, CurrentWeaponData);
		AimSpec.GetDynamicSpecSourceTags().AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick);
		GrantedWeaponAbilityHandles.Add(AbilitySystem->GiveAbility(AimSpec));
		GrantedInputTags.Add(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick);
	}
	for (const UFE_WeaponAttackData* AttackAction : CurrentWeaponData->Template.AttackActions)
	{
		if (!AttackAction || (AttackAction->InputTag.IsValid() && GrantedInputTags.Contains(AttackAction->InputTag)))
		{
			continue;
		}

		UClass* AbilityClass = AttackAction->AbilityClass.Get();
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

void UFE_EquipmentComponent::ClearWeaponAbilities()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (UAbilitySystemComponent* AbilitySystem = WeaponAbilitySystem.Get())
		{
			for (const FGameplayAbilitySpecHandle& Handle : GrantedWeaponAbilityHandles)
			{
				if (Handle.IsValid())
				{
					AbilitySystem->ClearAbility(Handle);
				}
			}
		}
	}
	GrantedWeaponAbilityHandles.Reset();
	WeaponAbilitySystem.Reset();
}

UFE_WeaponItemData* UFE_EquipmentComponent::FindWeaponDataForTag(FGameplayTag ItemTag) const
{
	for (const TSoftObjectPtr<UFE_WeaponItemData>& WeaponDataAsset : WeaponDataAssets)
	{
		UFE_WeaponItemData* WeaponData = WeaponDataAsset.Get();
		if (WeaponData && (WeaponData->HasItemTag(ItemTag) || WeaponData->Template.WeaponTags.HasTagExact(ItemTag)))
		{
			return WeaponData;
		}
	}

	return nullptr;
}
