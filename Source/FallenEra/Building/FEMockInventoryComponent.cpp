// Fill out your copyright notice in the Description page of Project Settings.

#include "FEMockInventoryComponent.h"
#include "FEBuildingTypes.h"

int32 UFEMockInventoryComponent::CountItems(FGameplayTag ItemTag) const
{
	const int32* Found = Items.Find(ItemTag);
	return Found ? *Found : 0;
}

int32 UFEMockInventoryComponent::RemoveItems(FGameplayTag ItemTag, int32 Count)
{
	int32* Found = Items.Find(ItemTag);
	if (Found == nullptr || Count <= 0)
	{
		return 0;
	}
	const int32 Removed = FMath::Min(*Found, Count);
	*Found -= Removed;

	// UI 가 없는 동안 재고 변화를 확인하는 용도. mock 과 함께 삭제된다.
	UE_LOG(LogFEBuilding, Log, TEXT("%s: %s -%d (now %d)"), *GetNameSafe(GetOwner()), *ItemTag.ToString(), Removed, *Found);
	return Removed;
}

void UFEMockInventoryComponent::AddItems(FGameplayTag ItemTag, int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	int32& Stock = Items.FindOrAdd(ItemTag);
	Stock += Count;
	UE_LOG(LogFEBuilding, Log, TEXT("%s: %s +%d (now %d)"), *GetNameSafe(GetOwner()), *ItemTag.ToString(), Count, Stock);
}
