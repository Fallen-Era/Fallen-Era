// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildPiece.h"
#include "FEBuildStorage.generated.h"

/**
 * 보관함. 완성 상태에서 E 로 패널을 열어 아이템을 넣고 뺀다.
 * Contents 한 항목 = 한 칸. 같은 종류도 MaxStackSize 를 넘으면 다음 칸을 쓴다.
 * 용량은 칸 수(Capacity). 상위 보관함은 C++ 를 건드리지 않고 BP 자식에서 Capacity 만 키운다.
 */
UCLASS()
class FALLENERA_API AFEBuildStorage : public AFEBuildPiece
{
	GENERATED_BODY()

public:
	/** [Server Only] 같은 종류의 덜 찬 칸부터 채우고, 남으면 빈 칸을 쓴다. 실제로 들어간 수를 돌려준다 */
	int32 StoreItems(FGameplayTag ItemTag, int32 Count);

	/** [Server Only] SlotIndex 칸을 통째로 꺼낸다. 그 칸의 종류가 ExpectedTag 가 아니면 0 (클라가 본 칸이 그사이 바뀜) */
	int32 TakeSlot(int32 SlotIndex, FGameplayTag ExpectedTag);

	UFUNCTION(BlueprintPure, Category = "FallenEra|Building|Storage")
	int32 GetCapacity() const;

	UFUNCTION(BlueprintPure, Category = "FallenEra|Building|Storage")
	int32 GetMaxStackSize() const;

	const TArray<FFEBuildItemCost>& GetContents() const;
	
	/** 내용물이 있으면 철거 불가. 붕괴·파괴는 막을 수 없어 내용물이 사라진다. 아이템 월드 스폰 API 가 오면 바닥에 떨군다 */
	virtual bool CanDemolish(FText& OutReason) const override;

	/** [Native] 내용물 변화. 서버·클라 모두. 저장고 패널이 행을 갱신할 때 쓴다 */
	FSimpleMulticastDelegate OnContentsChangedNative;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** 칸 수. 기초 보관함 10, 상위 보관함은 BP 자식에서 올린다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Storage", meta = (ClampMin = 1))
	int32 Capacity = 10;

	/** 한 칸에 쌓이는 최대 개수. 아이템 데이터에 아이템별 스택 수가 생기면 그 값을 쓴다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Storage", meta = (ClampMin = 1))
	int32 MaxStackSize = 999;

	/** 칸 목록. 한 항목 = 한 칸, 같은 종류가 여러 칸에 있을 수 있다. 빈 칸은 항목이 없는 것 */
	UPROPERTY(ReplicatedUsing = OnRep_Contents)
	TArray<FFEBuildItemCost> Contents;

	UFUNCTION()
	void OnRep_Contents();

	virtual bool CanInteractBuilt(AActor* InstigatorActor) const override;
	virtual FText GetInteractTextBuilt(AActor* InstigatorActor) const override;
	virtual void InteractBuiltLocal(AActor* InstigatorActor) override;

	virtual void WriteRecord(FFEBuildPieceRecord& OutRecord) const override;
	virtual void ReadRecord(const FFEBuildPieceRecord& Record) override;
};
