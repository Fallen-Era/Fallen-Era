#include "HT/Component/EquipmentComponent.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Interface/CombatPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "HT/Effect/WeaponOffenseGameplayEffect.h"
#include "HT/Projectile/CombatProjectile.h"
#include "HT/Weapon/WeaponItemData.h"
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
	RefreshEquipment();
}

void UFE_EquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFE_EquipmentComponent, CurrentItemTagIndex);
	DOREPLIFETIME(UFE_EquipmentComponent, EquippedArmor);
	DOREPLIFETIME(UFE_EquipmentComponent, bInitialArmorApplied);
}

void UFE_EquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bArmorEndingPlay = true;
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
	ClearArmorEffects();
	// PlayerState-owned ASCs may outlive the equipped character.
	ClearEquippedWeaponOffense();
	ClearWeaponAbilities();
	Super::EndPlay(EndPlayReason);
}

void UFE_EquipmentComponent::RefreshEquipment()
{
	EquipCurrentWeapon();
	RefreshArmorEquipment();
}

void UFE_EquipmentComponent::CycleWeapon()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
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

void UFE_EquipmentComponent::ServerCycleWeapon_Implementation()
{
	CycleWeapon();
}

void UFE_EquipmentComponent::EquipWeaponByItemTag(FGameplayTag ItemTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemTag.IsValid())
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
	if (WeaponLoadHandle)
	{
		WeaponLoadHandle->CancelHandle();
		WeaponLoadHandle.Reset();
	}
	ClearEquippedWeaponOffense();
	ClearWeaponAbilities();
	CurrentWeaponData = nullptr;
	PendingWeaponData = nullptr;
	ClearWeaponVisuals();

	const FGameplayTag CurrentTag = GetCurrentItemTag();
	if (!CurrentTag.IsValid())
	{
		WeaponChangedDelegate.Broadcast(nullptr);
		return;
	}

	TArray<FSoftObjectPath> UnloadedWeaponDataPaths;
	for (const TSoftObjectPtr<UFE_WeaponItemData>& WeaponDataAsset : WeaponDataAssets)
	{
		if (!WeaponDataAsset.IsNull() && !WeaponDataAsset.IsValid())
		{
			UnloadedWeaponDataPaths.AddUnique(WeaponDataAsset.ToSoftObjectPath());
		}
	}
	if (!UnloadedWeaponDataPaths.IsEmpty())
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
	WeaponLoadHandle.Reset();
	UFE_WeaponItemData* LoadedWeaponData = PendingWeaponData;
	if (bArmorEndingPlay)
	{
		return;
	}
	if (!LoadedWeaponData ||
		!(LoadedWeaponData->HasItemTag(GetCurrentItemTag()) || LoadedWeaponData->WeaponTags.HasTagExact(GetCurrentItemTag())))
	{
		EquipCurrentWeapon();
		return;
	}
	ApplyLoadedWeapon(LoadedWeaponData);
}

void UFE_EquipmentComponent::ApplyLoadedWeapon(UFE_WeaponItemData* WeaponData)
{
	CurrentWeaponData = WeaponData;
	PendingWeaponData = nullptr;

	ApplyEquippedWeaponOffense();
	ApplyWeaponVisuals();
	CacheWeaponAbilities();
	WeaponChangedDelegate.Broadcast(CurrentWeaponData);
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
	TArray<FSoftObjectPath>& OutPaths) const
{
	if (!WeaponData)
	{
		return;
	}
	auto AddUnloadedPath = [&OutPaths](const FSoftObjectPath& Path, bool bIsLoaded)
	{
		if (Path.IsValid() && !bIsLoaded)
		{
			OutPaths.AddUnique(Path);
		}
	};
	if (!WeaponData->SkeletalMesh.IsNull())
	{
		AddUnloadedPath(WeaponData->SkeletalMesh.ToSoftObjectPath(), WeaponData->SkeletalMesh.IsValid());
	}
	else
	{
		AddUnloadedPath(WeaponData->Mesh.ToSoftObjectPath(), WeaponData->Mesh.IsValid());
	}
	AddUnloadedPath(WeaponData->WeaponAnimLayerClass.ToSoftObjectPath(), WeaponData->WeaponAnimLayerClass.IsValid());
	AddUnloadedPath(WeaponData->CrosshairTexture.ToSoftObjectPath(), WeaponData->CrosshairTexture.IsValid());
	AddUnloadedPath(WeaponData->DefaultDamageEffect.ToSoftObjectPath(), WeaponData->DefaultDamageEffect.IsValid());
	for (const UFE_WeaponAttackData* AttackData : WeaponData->AttackActions)
	{
		if (!AttackData)
		{
			continue;
		}
		AddUnloadedPath(AttackData->AbilityClass.ToSoftObjectPath(), AttackData->AbilityClass.IsValid());
		AddUnloadedPath(AttackData->DamageEffectOverride.ToSoftObjectPath(), AttackData->DamageEffectOverride.IsValid());
		if (const UFE_ProjectileAttackDataBase* ProjectileData = Cast<UFE_ProjectileAttackDataBase>(AttackData))
		{
			AddUnloadedPath(ProjectileData->ProjectileClass.ToSoftObjectPath(), ProjectileData->ProjectileClass.IsValid());
		}
		if (const UFE_ChargedProjectileAttackData* ChargedData = Cast<UFE_ChargedProjectileAttackData>(AttackData))
		{
			AddUnloadedPath(
				ChargedData->TrajectoryNiagaraSystem.ToSoftObjectPath(),
				ChargedData->TrajectoryNiagaraSystem.IsValid());
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

	const float WeaponOffense = CurrentWeaponData->WeaponStat.Offense;

	if (WeaponOffense <= 0.0f)
	{
		return;
	}

	const FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	const FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
		UFE_WeaponOffenseGameplayEffect::StaticClass(), 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		return;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(FallenEraGameplayTags::SetByCaller_AttackPower, WeaponOffense);
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
	auto ClearMesh = [](UMeshComponent* MeshComponent)
	{
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
		{
			StaticMeshComponent->SetStaticMesh(nullptr);
		}
		else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent))
		{
			SkeletalMeshComponent->SetSkeletalMeshAsset(nullptr);
		}
	};
	ClearMesh(EquippedWorldWeaponMesh);
	ClearMesh(EquippedFirstPersonWeaponMesh);
	UpdateWeaponAnimLayers(nullptr);
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

	auto CreateMeshComponent = [CombatCharacter, bUseSkeletalMesh](
		const TCHAR* BaseName, USceneComponent* AttachParent, bool bFirstPerson) -> UMeshComponent*
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
		if (bFirstPerson)
		{
			NewMeshComponent->SetOnlyOwnerSee(true);
			NewMeshComponent->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		}
		else
		{
			NewMeshComponent->SetOwnerNoSee(IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter) != nullptr);
		}
		NewMeshComponent->RegisterComponent();
		return NewMeshComponent;
	};

	if (!EquippedWorldWeaponMesh)
	{
		EquippedWorldWeaponMesh = CreateMeshComponent(
			bUseSkeletalMesh ? TEXT("EquippedWorldWeaponSkeletalMesh") : TEXT("EquippedWorldWeaponStaticMesh"),
			CombatCharacter->GetMesh(), false);
	}
	if (!EquippedFirstPersonWeaponMesh)
	{
		EquippedFirstPersonWeaponMesh = CreateMeshComponent(
			bUseSkeletalMesh ? TEXT("EquippedFirstPersonWeaponSkeletalMesh") : TEXT("EquippedFirstPersonWeaponStaticMesh"),
			IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter), true);
	}
}

void UFE_EquipmentComponent::ApplyWeaponVisuals()
{
	if (!CurrentWeaponData)
	{
		ClearWeaponVisuals();
		return;
	}

	const ACharacter* CombatCharacter = Cast<ACharacter>(GetOwner());
	const bool bUseSkeletalMesh = !CurrentWeaponData->SkeletalMesh.IsNull();
	EnsureWeaponMeshComponents(bUseSkeletalMesh);

	auto ApplyMeshAsset = [this, bUseSkeletalMesh](UMeshComponent* MeshComponent)
	{
		if (bUseSkeletalMesh)
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent))
			{
				SkeletalMeshComponent->SetSkeletalMeshAsset(CurrentWeaponData->SkeletalMesh.Get());
			}
		}
		else if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
		{
			StaticMeshComponent->SetStaticMesh(CurrentWeaponData->Mesh.Get());
		}
	};
	if (EquippedWorldWeaponMesh && CombatCharacter)
	{
		EquippedWorldWeaponMesh->AttachToComponent(
			CombatCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			CurrentWeaponData->AttachSocketName);
		EquippedWorldWeaponMesh->SetRelativeTransform(CurrentWeaponData->SocketOffset);
		ApplyMeshAsset(EquippedWorldWeaponMesh);
	}

	if (EquippedFirstPersonWeaponMesh && CombatCharacter && IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter))
	{
		EquippedFirstPersonWeaponMesh->AttachToComponent(
			IFE_CombatPresentation::FindFirstPersonMesh(CombatCharacter), FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			CurrentWeaponData->AttachSocketName);
		EquippedFirstPersonWeaponMesh->SetRelativeTransform(CurrentWeaponData->SocketOffset);
		ApplyMeshAsset(EquippedFirstPersonWeaponMesh);
	}

	UpdateWeaponAnimLayers(CurrentWeaponData->WeaponAnimLayerClass.Get());
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
	for (const UFE_WeaponAttackData* AttackAction : CurrentWeaponData->AttackActions)
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
		if (WeaponData && (WeaponData->HasItemTag(ItemTag) || WeaponData->WeaponTags.HasTagExact(ItemTag)))
		{
			return WeaponData;
		}
	}

	return nullptr;
}
