#include "FEInventoryComponent.h"
#include "Net/UnrealNetwork.h"

UFEInventoryComponent::UFEInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UFEInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UFEInventoryComponent, Slots);
	DOREPLIFETIME(UFEInventoryComponent, ExpandedSlotCount);
	DOREPLIFETIME(UFEInventoryComponent, CurrentWeight);
	DOREPLIFETIME(UFEInventoryComponent, MaxWeight);
	DOREPLIFETIME(UFEInventoryComponent, SelectedSlotIndex);
}

void UFEInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Slots.Init(FItemInstance(),BaseSlotCount);
	}
}

int32 UFEInventoryComponent::GetTotalSlotCount() const
{
	return BaseSlotCount + ExpandedSlotCount;
}

bool UFEInventoryComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex); // 적합한 index인지 검증 (캡슐화)
}

const FItemInstance& UFEInventoryComponent::GetItemAt(int32 SlotIndex) const
{
	if (!IsValidSlotIndex(SlotIndex)) // 검증 후 이상이 있다면 빈 FItemInstance반환
	{
		static const FItemInstance EmptyItem;
		return EmptyItem;
	}
		
	return Slots[SlotIndex];
}

bool UFEInventoryComponent::IsOverweight() const
{
	return MaxWeight < CurrentWeight;
}



