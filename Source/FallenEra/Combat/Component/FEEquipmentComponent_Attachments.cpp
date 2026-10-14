#include "Combat/Component/FEEquipmentComponent.h"

#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEWeaponAimingComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/GameplayEffect/FEWeaponOffenseGameplayEffect.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/Weapon/Attachment/FEWeaponSightData.h"
#include "Combat/UI/FEWeaponScopeWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"

bool UFE_EquipmentComponent::SetWeaponAttachments(const TArray<FFE_WeaponAttachmentInstance>& Attachments)
{
	const UFE_RangedWeaponItemData* Weapon = Cast<UFE_RangedWeaponItemData>(CurrentWeaponData);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Weapon || !Weapon->bSupportsAiming || !CanChangeEquipment())
	{
		return false;
	}
	for (const auto& Entry : Attachments)
	{
		if (Entry.Definition.IsNull() || (Entry.bOverrideAccuracyBonus && !FMath::IsFinite(Entry.AccuracyBonus)))
		{
			return false;
		}
	}
	WeaponAttachmentLoadout.WeaponTag = GetCurrentItemTag();
	WeaponAttachmentLoadout.Attachments = Attachments;
	LastEquipmentChangeTime = GetWorld()->GetTimeSeconds();
	GetOwner()->ForceNetUpdate();
	RefreshWeaponAttachments();
	return true;
}

void UFE_EquipmentComponent::OnRep_WeaponAttachments()
{
	RefreshWeaponAttachments();
}

void UFE_EquipmentComponent::InitializeWeaponAttachments()
{
	const UFE_RangedWeaponItemData* Weapon = Cast<UFE_RangedWeaponItemData>(CurrentWeaponData);
	if (GetOwner()->HasAuthority() && WeaponAttachmentLoadout.WeaponTag != GetCurrentItemTag())
	{
		WeaponAttachmentLoadout.WeaponTag = GetCurrentItemTag();
		WeaponAttachmentLoadout.Attachments.Reset();
		if (Weapon && Weapon->bSupportsAiming)
		{
			for (const auto& Definition : Weapon->DefaultAttachments)
			{
				if (!Definition.IsNull())
				{
					FFE_WeaponAttachmentInstance& Entry = WeaponAttachmentLoadout.Attachments.AddDefaulted_GetRef();
					Entry.Definition = Definition;
				}
			}
		}
		GetOwner()->ForceNetUpdate();
	}
	RefreshWeaponAttachments();
}

void UFE_EquipmentComponent::ClearWeaponAttachments()
{
	// A released ability may already have ended while its local blend-out is still running.
	if (UFE_WeaponAimingComponent* Aiming = GetOwner()->FindComponentByClass<UFE_WeaponAimingComponent>())
	{
		Aiming->EndAim(true);
	}
	if (UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner()))
	{
		FGameplayTagContainer Tags(FallenEraCombatGameplayTags::Ability_Combat_Aim);
		ASC->CancelAbilities(&Tags);
	}
	if (AttachmentLoadHandle) { AttachmentLoadHandle->CancelHandle(); AttachmentLoadHandle.Reset(); }
	if (GetOwner()->HasAuthority() && AttachmentStatsEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = AttachmentAbilitySystem.Get())
		{
			ASC->RemoveActiveGameplayEffect(AttachmentStatsEffectHandle);
		}
	}
	AttachmentStatsEffectHandle = FActiveGameplayEffectHandle();
	AttachmentAbilitySystem.Reset();
	for (UMeshComponent* Mesh : WeaponAttachmentMeshes)
	{
		if (Mesh) { Mesh->DestroyComponent(); }
	}
	WeaponAttachmentMeshes.Reset();
	FirstPersonSightMesh = nullptr;
	LoadedAttachmentDefinitions.Reset();
	ActiveSightData = nullptr;
	bWeaponAttachmentsReady = false;
	bLoadingAttachmentDependencies = false;
	WeaponAttachmentsChangedDelegate.Broadcast(CurrentWeaponData);
}

void UFE_EquipmentComponent::RefreshWeaponAttachments()
{
	ClearWeaponAttachments();
	const UFE_RangedWeaponItemData* Weapon = Cast<UFE_RangedWeaponItemData>(CurrentWeaponData);
	if (bArmorEndingPlay || !Weapon || !Weapon->bSupportsAiming || WeaponAttachmentLoadout.WeaponTag != GetCurrentItemTag())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const auto& Entry : WeaponAttachmentLoadout.Attachments)
	{
		if (!Entry.Definition.IsValid()) { Paths.AddUnique(Entry.Definition.ToSoftObjectPath()); }
	}
	if (!Weapon->DefaultSight.IsNull() && !Weapon->DefaultSight.IsValid())
	{
		Paths.AddUnique(Weapon->DefaultSight.ToSoftObjectPath());
	}
	if (Paths.IsEmpty()) { FinishLoadingWeaponAttachments(); return; }
	AttachmentLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingWeaponAttachments));
}

void UFE_EquipmentComponent::FinishLoadingWeaponAttachments()
{
	const UFE_RangedWeaponItemData* Weapon = Cast<UFE_RangedWeaponItemData>(CurrentWeaponData);
	if (bArmorEndingPlay || !Weapon || WeaponAttachmentLoadout.WeaponTag != GetCurrentItemTag()) { return; }
	TSet<FGameplayTag> Slots;
	LoadedAttachmentDefinitions.Reset();
	ActiveSightData = nullptr;
	TArray<FSoftObjectPath> Paths;
	bool bAllLoaded = true;
	for (const auto& Entry : WeaponAttachmentLoadout.Attachments)
	{
		UFE_WeaponAttachmentData* Definition = Entry.Definition.Get();
		if (!Definition || !Definition->SlotTag.IsValid() || Slots.Contains(Definition->SlotTag))
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid or duplicate attachment slot on %s"), *GetNameSafe(Weapon));
			return;
		}
		Slots.Add(Definition->SlotTag);
		LoadedAttachmentDefinitions.Add(Definition);
		Paths.AddUnique(Entry.Definition.ToSoftObjectPath());
		if (UFE_WeaponSightData* Sight = Cast<UFE_WeaponSightData>(Definition))
		{
			if (Sight->SlotTag != FallenEraCombatGameplayTags::Equipment_WeaponSlot_Sight) { return; }
			ActiveSightData = Sight;
		}
		if (!Definition->SkeletalMesh.IsNull())
		{
			Paths.AddUnique(Definition->SkeletalMesh.ToSoftObjectPath());
			bAllLoaded &= Definition->SkeletalMesh.IsValid();
		}
		else if (!Definition->Mesh.IsNull())
		{
			Paths.AddUnique(Definition->Mesh.ToSoftObjectPath());
			bAllLoaded &= Definition->Mesh.IsValid();
		}
	}
	if (!Weapon->DefaultSight.IsNull())
	{
		Paths.AddUnique(Weapon->DefaultSight.ToSoftObjectPath());
		if (!ActiveSightData) { ActiveSightData = Weapon->DefaultSight.Get(); }
	}
	if (ActiveSightData)
	{
		if (!ActiveSightData->ScopeWidgetClass.IsNull())
		{
			Paths.AddUnique(ActiveSightData->ScopeWidgetClass.ToSoftObjectPath());
			bAllLoaded &= ActiveSightData->ScopeWidgetClass.IsValid();
		}
		if (!ActiveSightData->ReticleTexture.IsNull())
		{
			Paths.AddUnique(ActiveSightData->ReticleTexture.ToSoftObjectPath());
			bAllLoaded &= ActiveSightData->ReticleTexture.IsValid();
		}
	}
	if (!bAllLoaded)
	{
		if (bLoadingAttachmentDependencies)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to load attachment presentation for %s"), *GetNameSafe(Weapon));
			return;
		}
		bLoadingAttachmentDependencies = true;
		AttachmentLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
			FStreamableDelegate::CreateUObject(this, &ThisClass::FinishLoadingWeaponAttachments));
		return;
	}
	// Keep even already-loaded soft dependencies alive for the equipped weapon's lifetime.
	if (!bLoadingAttachmentDependencies && !Paths.IsEmpty())
	{
		AttachmentLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}
	bWeaponAttachmentsReady = true;
	ApplyWeaponAttachmentVisuals();
	ApplyWeaponAttachmentStats();
	WeaponAttachmentsChangedDelegate.Broadcast(CurrentWeaponData);
}

void UFE_EquipmentComponent::ApplyWeaponAttachmentVisuals()
{
	for (const UFE_WeaponAttachmentData* Definition : LoadedAttachmentDefinitions)
	{
		const bool bUseSkeletalMesh = !Definition->SkeletalMesh.IsNull();
		if (bUseSkeletalMesh ? !Definition->SkeletalMesh.Get() : !Definition->Mesh.Get()) { continue; }
		auto CreateMesh = [this, Definition](UMeshComponent* Parent, bool bFirstPerson)
		{
			if (!Parent) { return; }
			UMeshComponent* Mesh;
			if (!Definition->SkeletalMesh.IsNull())
			{
				USkeletalMeshComponent* SkeletalMesh = NewObject<USkeletalMeshComponent>(GetOwner());
				SkeletalMesh->SetSkeletalMeshAsset(Definition->SkeletalMesh.Get());
				Mesh = SkeletalMesh;
			}
			else
			{
				UStaticMeshComponent* StaticMesh = NewObject<UStaticMeshComponent>(GetOwner());
				StaticMesh->SetStaticMesh(Definition->Mesh.Get());
				Mesh = StaticMesh;
			}
			GetOwner()->AddInstanceComponent(Mesh);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetGenerateOverlapEvents(false);
			Mesh->SetupAttachment(Parent, Definition->AttachSocketName);
			Mesh->SetRelativeTransform(Definition->SocketOffset);
			Mesh->SetOnlyOwnerSee(bFirstPerson);
			Mesh->SetOwnerNoSee(!bFirstPerson && EquippedFirstPersonWeaponMesh != nullptr);
			Mesh->FirstPersonPrimitiveType = Parent->FirstPersonPrimitiveType;
			Mesh->RegisterComponent();
			WeaponAttachmentMeshes.Add(Mesh);
			if (bFirstPerson && Definition == ActiveSightData) { FirstPersonSightMesh = Mesh; }
		};
		CreateMesh(EquippedWorldWeaponMesh, false);
		CreateMesh(EquippedFirstPersonWeaponMesh, true);
	}
}

UMeshComponent* UFE_EquipmentComponent::GetFirstPersonSightMesh() const
{
	const bool bHasMountedSight = LoadedAttachmentDefinitions.ContainsByPredicate(
		[this](const UFE_WeaponAttachmentData* Definition) { return Definition == ActiveSightData; });
	// A mounted sight without a usable mesh is a configuration error, not a built-in sight.
	return bHasMountedSight ? FirstPersonSightMesh.Get() : EquippedFirstPersonWeaponMesh.Get();
}

void UFE_EquipmentComponent::ApplyWeaponAttachmentStats()
{
	if (!GetOwner()->HasAuthority()) { return; }
	UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	if (!ASC) { return; }
	float Bonus = 0.0f;
	for (const auto& Entry : WeaponAttachmentLoadout.Attachments) { Bonus += Entry.ResolveAccuracyBonus(); }
	const bool bHasMountedSight = LoadedAttachmentDefinitions.ContainsByPredicate(
		[this](const UFE_WeaponAttachmentData* Definition) { return Definition == ActiveSightData; });
	if (ActiveSightData && !bHasMountedSight)
	{
		// Built-in sight replaces, rather than stacks with, an installed sight's bonus.
		FFE_WeaponAttachmentInstance DefaultSightInstance;
		DefaultSightInstance.Definition = ActiveSightData.Get();
		Bonus += DefaultSightInstance.ResolveAccuracyBonus();
	}
	if (FMath::IsNearlyZero(Bonus)) { return; }
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		UFE_WeaponOffenseGameplayEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
	if (!Spec.IsValid()) { return; }
	Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_AttackPower, 0.0f);
	Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_Accuracy, Bonus);
	Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_RecoilControl, 0.0f);
	AttachmentAbilitySystem = ASC;
	AttachmentStatsEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}
