#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FEItemType.h"               // FItemInstance 인식
#include "FEBuildInventoryProvider.h" // 팀원 건축 인터페이스 상속용
#include "FEInventoryComponent.generated.h"

UCLASS(ClassGroup = (FallenEra), meta = (BlueprintSpawnableComponent))
class FALLENERA_API UFEInventoryComponent : public UActorComponent, public IFEBuildInventoryProvider
{
	GENERATED_BODY()
	
	// 1. 전체 슬롯 배열 (복제)
	UPROPERTY(ReplicatedUsing = OnRep_Slots, VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Inventory",meta = (AllowPrivateAccess = "true"))
	TArray<FItemInstance> Slots;

	// 2. 기본 슬롯 수 (런타임에 안 바뀌므로 Replicate 불필요, 에디터 기본값 30)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Inventory",meta = (AllowPrivateAccess = "true"))
	int32 BaseSlotCount = 30;

	// 3. 가방 장착으로 열린 추가 슬롯 수 (복제)
	UPROPERTY(ReplicatedUsing = OnRep_ExpandedSlotCount, BlueprintReadOnly, Category = "FallenEra|Inventory",meta = (AllowPrivateAccess = "true"))
	int32 ExpandedSlotCount = 0;

	// 4. 현재 무게 & 최대 허용 무게 (복제)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeight, BlueprintReadOnly, Category = "FallenEra|Inventory",meta = (AllowPrivateAccess = "true"))
	float CurrentWeight = 0.0f;
	
	UPROPERTY(ReplicatedUsing = OnRep_MaxWeight, BlueprintReadOnly, Category = "FallenEra|Inventory",meta = (AllowPrivateAccess = "true"))
	float MaxWeight = 100.0f;

	// 5. 현재 선택된 퀵슬롯 번호 (복제)
	UPROPERTY(ReplicatedUsing = OnRep_SelectedSlotIndex, BlueprintReadOnly, Category = "FallenEra|Inventory",meta = (AllowPrivateAccess = "true"))
	int32 SelectedSlotIndex = 0;
	
public:
	// LifeCycle & Replicate
	UFEInventoryComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 현재 열려있는 (사용가능한) 총 용량을 반환 (Capacity)
	UFUNCTION(BlueprintPure, Category = "FallenEra|Inventory")
	int32 GetTotalSlotCount() const;
	
	// 적합한 슬롯의 인덱스 값인지 판단해서 bool 반환
	UFUNCTION(BlueprintPure, Category = "FallenEra|Inventory")
	bool IsValidSlotIndex(int32 SlotIndex) const;
	
	// 아이템을 줍고 인벤토리에 추가하는 기능
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FallenEra|Inventory")
	bool AddItem(FItemInstance NewItem, int32& OutRemainingCount);
	
	// 슬롯 인덱스로 해당 슬롯의 아이템을 읽어들임
	UFUNCTION(BlueprintPure, Category = "FallenEra|Inventory")
	const FItemInstance& GetItemAt(int32 SlotIndex) const;
	
	// 인벤토리 내에서의 아이템 슬롯 변경 (이동, 교환, 병합)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FallenEra|Inventory")
	bool MoveOrSwapSlot(int32 FromSlotIndex, int32 ToSlotIndex);
	
	// 인벤토리 내에서 아이템 제거 및 소모
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FallenEra|Inventory")
	bool RemoveItemAt(int32 SlotIndex, int32 Count);
	
	// 무게 과적인지 판단
	UFUNCTION(BlueprintPure, Category = "FallenEra|Inventory")
	bool IsOverweight() const;
	
	// FEBuildInventoryProvider 상속
	int32 CountItems(FGameplayTag ItemTag) const override;
	int32 RemoveItems(FGameplayTag ItemTag, int32 Count) override;
	int32 AddItems(FGameplayTag ItemTag, int32 Count) override;
	void  GetItems(TArray<FFEBuildItemCost>& OutItems) const override;

protected:
	
	// 슬롯 변경이 있을 때마다 무게 갱신
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FallenEra|Inventory")
	void RefreshWeight();
	
	
	UFUNCTION()
	void OnRep_Slots();
	
	UFUNCTION()
	void OnRep_ExpandedSlotCount();
	
	UFUNCTION()
	void OnRep_CurrentWeight();
	
	UFUNCTION()
	void OnRep_MaxWeight();
	
	UFUNCTION()
	void OnRep_SelectedSlotIndex();
};
