#include "Combat/Component/FEEquipmentComponent.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Combat/Armor/FEArmorItemData.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/GameplayEffect/FEArmorStatsGameplayEffect.h"

void UFE_EquipmentComponent::RefreshArmorEquipment()
{
	if (bArmorEndingPlay || bInitialArmorInitializationDeferred || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	if (!ASC || ASC->GetAvatarActor() != GetOwner() || !ASC->HasAttributeSetForAttribute(UFallenEraAttributeSet::GetDefensePowerAttribute()))
	{
		return;
	}
	if (ArmorAbilitySystem.Get() != ASC)
	{
		// Remove from the old ASC, not from a newly possessed character's ASC.
		ClearArmorEffects();
		ArmorAbilitySystem = ASC;
	}
	for (UFE_ArmorItemData* Armor : EquippedArmor)
	{
		if (Armor && !ArmorEffectHandles.Contains(Armor->Template.EquipmentSlotTag))
		{
			FFE_ArmorInstanceData* Instance = ArmorInstances.Find(Armor->Template.EquipmentSlotTag);
			if (!Instance)
			{
				Instance = &ArmorInstances.Add(Armor->Template.EquipmentSlotTag, Armor->Instance);
				if (!Instance->InstanceId.IsValid()) { Instance->InstanceId = FGuid::NewGuid(); }
			}
			const FActiveGameplayEffectHandle Handle = ApplyArmorEffect(Armor, *Instance);
			if (Handle.IsValid())
			{
				ArmorEffectHandles.Add(Armor->Template.EquipmentSlotTag, Handle);
			}
		}
	}
	if (bInitialArmorApplied || bArmorLoadRequested)
	{
		return;
	}
	bArmorLoadRequested = true;
	TArray<FSoftObjectPath> Paths;
	bool bAllArmorLoaded = true;
	for (const TSoftObjectPtr<UFE_ArmorItemData>& Armor : DefaultArmorDataAssets)
	{
		if (!Armor.IsNull())
		{
			Paths.AddUnique(Armor.ToSoftObjectPath());
			bAllArmorLoaded &= Armor.IsValid();
		}
	}
	if (bAllArmorLoaded)
	{
		FinishLoadingDefaultArmor();
		return;
	}
	ArmorLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &UFE_EquipmentComponent::FinishLoadingDefaultArmor));
}

void UFE_EquipmentComponent::FinishLoadingDefaultArmor()
{
	if (bArmorEndingPlay || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bArmorLoadRequested = false;
	UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(GetOwner());
	if (!ASC || ASC->GetAvatarActor() != GetOwner())
	{
		ArmorLoadHandle.Reset();
		return; // Retry after the next ASC-ready RefreshEquipment call.
	}
	bInitialArmorApplied = true;
	for (const TSoftObjectPtr<UFE_ArmorItemData>& Reference : DefaultArmorDataAssets)
	{
		UFE_ArmorItemData* Armor = Reference.Get();
		// First valid entry wins if multiple defaults specify the same slot.
		if (Armor && !GetEquippedArmor(Armor->Template.EquipmentSlotTag))
		{
			EquipArmor(Armor);
		}
	}
	ArmorLoadHandle.Reset();
}

const UFE_ArmorItemData* UFE_EquipmentComponent::GetEquippedArmor(FGameplayTag SlotTag) const
{
	for (const UFE_ArmorItemData* Armor : EquippedArmor)
	{
		if (Armor && Armor->Template.EquipmentSlotTag == SlotTag)
		{
			return Armor;
		}
	}
	return nullptr;
}

FFE_ArmorInstanceData UFE_EquipmentComponent::GetArmorInstanceData(FGameplayTag SlotTag) const
{
	const FFE_ArmorInstanceData* Instance = ArmorInstances.Find(SlotTag);
	return Instance ? *Instance : FFE_ArmorInstanceData();
}

FActiveGameplayEffectHandle UFE_EquipmentComponent::ApplyArmorEffect(UFE_ArmorItemData* ArmorData, const FFE_ArmorInstanceData& ArmorInstance)
{
	UAbilitySystemComponent* ASC = ArmorAbilitySystem.Get();
	if (!ASC || !ArmorData || !ArmorInstance.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(ArmorData);
	const FFE_ArmorStat& ArmorStats = ArmorInstance.ResolveStats();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UFE_ArmorStatsGameplayEffect::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_DefensePower, ArmorStats.Defense);
	Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_KnockbackResistance, ArmorStats.KnockbackResistance);
	return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

bool UFE_EquipmentComponent::EquipArmor(UFE_ArmorItemData* ArmorData)
{
	return ArmorData && EquipArmorInstance(ArmorData, ArmorData->Instance);
}

bool UFE_EquipmentComponent::EquipArmorInstance(UFE_ArmorItemData* ArmorData, const FFE_ArmorInstanceData& NewInstance)
{
	if (bArmorEndingPlay || !GetOwner() || !GetOwner()->HasAuthority() || !bInitialArmorApplied ||
		!ArmorData || !ArmorData->Template.EquipmentSlotTag.IsValid() ||
		!NewInstance.IsValid())
	{
		return false;
	}
	RefreshArmorEquipment();
	if (!ArmorAbilitySystem.IsValid() || ArmorAbilitySystem->GetAvatarActor() != GetOwner())
	{
		return false;
	}
	// Apply first so a rejected effect does not discard the previous equipment.
	FFE_ArmorInstanceData Snapshot = NewInstance;
	if (!Snapshot.InstanceId.IsValid()) { Snapshot.InstanceId = FGuid::NewGuid(); }
	const FActiveGameplayEffectHandle NewHandle = ApplyArmorEffect(ArmorData, Snapshot);
	if (!NewHandle.IsValid())
	{
		return false;
	}
	UnequipArmor(ArmorData->Template.EquipmentSlotTag);
	EquippedArmor.Add(ArmorData);
	ArmorInstances.Add(ArmorData->Template.EquipmentSlotTag, Snapshot);
	ArmorEffectHandles.Add(ArmorData->Template.EquipmentSlotTag, NewHandle);
	return true;
}

bool UFE_EquipmentComponent::UnequipArmor(FGameplayTag SlotTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bInitialArmorApplied)
	{
		return false;
	}
	if (const FActiveGameplayEffectHandle* Handle = ArmorEffectHandles.Find(SlotTag))
	{
		if (UAbilitySystemComponent* ASC = ArmorAbilitySystem.Get())
		{
			ASC->RemoveActiveGameplayEffect(*Handle);
		}
		ArmorEffectHandles.Remove(SlotTag);
	}
	ArmorInstances.Remove(SlotTag);
	return EquippedArmor.RemoveAll([SlotTag](const UFE_ArmorItemData* Armor)
	{
		return Armor && Armor->Template.EquipmentSlotTag == SlotTag;
	}) > 0;
}

void UFE_EquipmentComponent::ClearArmorEffects()
{
	if (UAbilitySystemComponent* ASC = ArmorAbilitySystem.Get())
	{
		for (const TPair<FGameplayTag, FActiveGameplayEffectHandle>& Entry : ArmorEffectHandles)
		{
			ASC->RemoveActiveGameplayEffect(Entry.Value);
		}
	}
	ArmorEffectHandles.Reset();
	ArmorAbilitySystem.Reset();
}
