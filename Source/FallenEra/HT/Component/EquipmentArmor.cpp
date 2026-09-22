#include "HT/Component/EquipmentComponent.h"

#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "HT/Armor/ArmorItemData.h"
#include "HT/Armor/ArmorGameplayTags.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Effect/ArmorStatsGameplayEffect.h"

void UFE_EquipmentComponent::RefreshArmorEquipment()
{
	if (bArmorEndingPlay || !GetOwner() || !GetOwner()->HasAuthority())
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
		if (Armor && !ArmorEffectHandles.Contains(Armor->EquipmentSlotTag))
		{
			const FActiveGameplayEffectHandle Handle = ApplyArmorEffect(Armor);
			if (Handle.IsValid())
			{
				ArmorEffectHandles.Add(Armor->EquipmentSlotTag, Handle);
			}
		}
	}
	if (bInitialArmorApplied || bArmorLoadRequested)
	{
		return;
	}
	bArmorLoadRequested = true;
	TArray<FSoftObjectPath> Paths;
	for (const TSoftObjectPtr<UFE_ArmorItemData>& Armor : DefaultArmorDataAssets)
	{
		if (!Armor.IsNull())
		{
			Paths.AddUnique(Armor.ToSoftObjectPath());
		}
	}
	if (Paths.IsEmpty())
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
		if (Armor && !GetEquippedArmor(Armor->EquipmentSlotTag))
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
		if (Armor && Armor->EquipmentSlotTag == SlotTag)
		{
			return Armor;
		}
	}
	return nullptr;
}

FActiveGameplayEffectHandle UFE_EquipmentComponent::ApplyArmorEffect(UFE_ArmorItemData* ArmorData)
{
	UAbilitySystemComponent* ASC = ArmorAbilitySystem.Get();
	if (!ASC || !ArmorData)
	{
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(ArmorData);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UFE_ArmorStatsGameplayEffect::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	Spec.Data->SetSetByCallerMagnitude(FE_ArmorGameplayTags::SetByCaller_DefensePower, FMath::Max(ArmorData->ArmorStat.Defense, 0.0f));
	Spec.Data->SetSetByCallerMagnitude(FE_ArmorGameplayTags::SetByCaller_KnockbackResistance, FMath::Max(ArmorData->ArmorStat.KnockbackResistance, 0.0f));
	return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

bool UFE_EquipmentComponent::EquipArmor(UFE_ArmorItemData* ArmorData)
{
	if (bArmorEndingPlay || !GetOwner() || !GetOwner()->HasAuthority() || !bInitialArmorApplied ||
		!ArmorData || !ArmorData->EquipmentSlotTag.IsValid() ||
		!FMath::IsFinite(ArmorData->ArmorStat.Defense) || !FMath::IsFinite(ArmorData->ArmorStat.KnockbackResistance))
	{
		return false;
	}
	RefreshArmorEquipment();
	if (!ArmorAbilitySystem.IsValid() || ArmorAbilitySystem->GetAvatarActor() != GetOwner())
	{
		return false;
	}
	// Apply first so a rejected effect does not discard the previous equipment.
	const FActiveGameplayEffectHandle NewHandle = ApplyArmorEffect(ArmorData);
	if (!NewHandle.IsValid())
	{
		return false;
	}
	UnequipArmor(ArmorData->EquipmentSlotTag);
	EquippedArmor.Add(ArmorData);
	ArmorEffectHandles.Add(ArmorData->EquipmentSlotTag, NewHandle);
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
	return EquippedArmor.RemoveAll([SlotTag](const UFE_ArmorItemData* Armor)
	{
		return Armor && Armor->EquipmentSlotTag == SlotTag;
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
