// Fill out your copyright notice in the Description page of Project Settings.

#include "FEItemManagerSubsystem.h"
#include "FEItemBase.h"
#include "AssetRegistry/AssetRegistryModule.h"

DEFINE_LOG_CATEGORY(LogFEItemManager);

UFEItemManagerSubsystem::UFEItemManagerSubsystem()
{
}

void UFEItemManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogFEItemManager, Log, TEXT("UFEItemManagerSubsystem Initialized. Building Item Cache..."));
	RefreshItemCache();
}

void UFEItemManagerSubsystem::Deinitialize()
{
	ItemCache.Empty();
	Super::Deinitialize();
}

void UFEItemManagerSubsystem::RefreshItemCache()
{
	ItemCache.Empty();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	// UFEItemBase 및 그 모든 파생 클래스를 AssetRegistry에서 스캔
	AssetRegistry.GetAssetsByClass(UFEItemBase::StaticClass()->GetClassPathName(), AssetDataList, true);

	for (const FAssetData& AssetData : AssetDataList)
	{
		const UFEItemBase* ItemAsset = Cast<UFEItemBase>(AssetData.GetAsset());
		if (!ItemAsset)
		{
			ItemAsset = Cast<UFEItemBase>(AssetData.ToSoftObjectPath().TryLoad());
		}

		if (ItemAsset)
		{
			if (!ItemAsset->ItemID.IsNone())
			{
				if (ItemCache.Contains(ItemAsset->ItemID))
				{
					UE_LOG(LogFEItemManager, Warning, TEXT("Duplicate ItemID found: '%s' in asset '%s' (Already registered in '%s')"),
						*ItemAsset->ItemID.ToString(),
						*ItemAsset->GetName(),
						*ItemCache[ItemAsset->ItemID]->GetName());
				}
				ItemCache.Add(ItemAsset->ItemID, ItemAsset);
				UE_LOG(LogFEItemManager, Verbose, TEXT("Cached Item: [%s] -> %s"), *ItemAsset->ItemID.ToString(), *ItemAsset->GetName());
			}
			else
			{
				UE_LOG(LogFEItemManager, Warning, TEXT("Item Asset '%s' has an empty ItemID! Please configure ItemID in DataAsset."), *ItemAsset->GetName());
			}
		}
	}

	UE_LOG(LogFEItemManager, Log, TEXT("Item Cache successfully built. Total cached items: %d"), ItemCache.Num());
}

const UFEItemBase* UFEItemManagerSubsystem::GetItemData(FName InItemID) const
{
	if (InItemID.IsNone())
	{
		return nullptr;
	}

	const TObjectPtr<const UFEItemBase>* Found = ItemCache.Find(InItemID);
	return Found ? Found->Get() : nullptr;
}

const UFEItemBase* UFEItemManagerSubsystem::GetItemDataByPrimaryId(FPrimaryAssetId InAssetId) const
{
	if (!InAssetId.IsValid())
	{
		return nullptr;
	}

	return GetItemData(InAssetId.PrimaryAssetName);
}

bool UFEItemManagerSubsystem::HasItemData(FName InItemID) const
{
	return !InItemID.IsNone() && ItemCache.Contains(InItemID);
}

TArray<const UFEItemBase*> UFEItemManagerSubsystem::GetAllItemData() const
{
	TArray<const UFEItemBase*> Result;
	Result.Reserve(ItemCache.Num());
	for (const auto& Pair : ItemCache)
	{
		if (Pair.Value)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<const UFEItemBase*> UFEItemManagerSubsystem::GetItemsByTag(FGameplayTag MatchingTag) const
{
	TArray<const UFEItemBase*> Result;
	for (const auto& Pair : ItemCache)
	{
		if (Pair.Value && Pair.Value->ItemTags.HasTag(MatchingTag))
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}
