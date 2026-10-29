// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildStorage.h"
#include "FEBuildingComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

void AFEBuildStorage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFEBuildStorage, Contents);
}

int32 AFEBuildStorage::StoreItems(FGameplayTag ItemTag, int32 Count)
{
	if (!HasAuthority() || !ItemTag.IsValid() || Count <= 0)
	{
		return 0;
	}

	int32 Remaining = Count;

	// 1) 같은 종류의 덜 찬 칸부터 채운다
	for (FFEBuildItemCost& Slot : Contents)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Slot.ItemTag == ItemTag && Slot.Count < MaxStackSize)
		{
			const int32 Added = FMath::Min(Remaining, MaxStackSize - Slot.Count);
			Slot.Count += Added;
			Remaining -= Added;
		}
	}

	// 2) 남으면 빈 칸을 새로 쓴다. 칸이 없으면 나머지는 거부 (호출자가 인벤토리에 남겨 둔다)
	while (Remaining > 0 && Contents.Num() < Capacity)
	{
		FFEBuildItemCost& Slot = Contents.AddDefaulted_GetRef();
		Slot.ItemTag = ItemTag;
		Slot.Count = FMath::Min(Remaining, MaxStackSize);
		Remaining -= Slot.Count;
	}

	const int32 Stored = Count - Remaining;
	if (Stored > 0)
	{
		OnRep_Contents(); // 서버 로컬 반영
	}
	
	return Stored;
}

int32 AFEBuildStorage::TakeSlot(int32 SlotIndex, FGameplayTag ExpectedTag)
{
	// 다른 플레이어가 먼저 꺼내 칸이 당겨졌을 수 있다. 종류가 다르면 엉뚱한 칸을 꺼내지 않도록 거부
	const bool bIsValidSlot = Contents.IsValidIndex(SlotIndex) && Contents[SlotIndex].ItemTag == ExpectedTag;
	if (!HasAuthority() || !bIsValidSlot)
	{
		return 0;
	}

	const int32 Taken = Contents[SlotIndex].Count;
	Contents.RemoveAt(SlotIndex); // 뒤 칸들이 앞으로 당겨진다 (빈 칸은 항상 뒤쪽)
	OnRep_Contents();
	
	return Taken;
}

int32 AFEBuildStorage::GetCapacity() const
{
	return Capacity;
}

int32 AFEBuildStorage::GetMaxStackSize() const
{
	return MaxStackSize;
}

const TArray<FFEBuildItemCost>& AFEBuildStorage::GetContents() const
{
	return Contents;
}

bool AFEBuildStorage::CanDemolish(FText& OutReason) const
{
	if (Contents.Num() > 0)
	{
		OutReason = LOCTEXT("StorageNotEmpty", "보관함을 비워야 철거할 수 있습니다");
		return false;
	}
	return true;
}

void AFEBuildStorage::OnRep_Contents()
{
	OnContentsChangedNative.Broadcast();
}

bool AFEBuildStorage::CanInteractBuilt(AActor* InstigatorActor) const
{
	return true;
}

FText AFEBuildStorage::GetInteractTextBuilt(AActor* InstigatorActor) const
{
	// 남은 칸은 패널의 빈 칸으로 보여 준다. 프롬프트에는 숫자를 넣지 않는다
	return LOCTEXT("StoragePrompt", "보관함 열기");
}

void AFEBuildStorage::InteractBuiltLocal(AActor* InstigatorActor)
{
	// 패널은 요청한 클라이언트에서만 열린다. 실제 이동은 그쪽 건설 컴포넌트의 서버 RPC 로 간다.
	if (UFEBuildingComponent* Building = InstigatorActor ? InstigatorActor->FindComponentByClass<UFEBuildingComponent>() : nullptr)
	{
		Building->OpenStoragePanel(this);
	}
}

void AFEBuildStorage::WriteRecord(FFEBuildPieceRecord& OutRecord) const
{
	Super::WriteRecord(OutRecord);
	OutRecord.Items = Contents;
}

void AFEBuildStorage::ReadRecord(const FFEBuildPieceRecord& Record)
{
	Super::ReadRecord(Record);
	Contents = Record.Items;
	OnRep_Contents();
}

#undef LOCTEXT_NAMESPACE