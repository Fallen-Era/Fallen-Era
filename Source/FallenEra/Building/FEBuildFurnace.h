// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildStorage.h"
#include "FEBuildFurnace.generated.h"

class UPointLightComponent;

/** 화로 레시피 1개: 재료 1개 → 결과 1개 */
USTRUCT(BlueprintType)
struct FALLENERA_API FFEFurnaceRecipe
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Furnace", meta = (Categories = "Item"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Furnace", meta = (Categories = "Item"))
	FGameplayTag OutputTag;

	/** 재료 1개를 가공하는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Furnace", meta = (ClampMin = 0.5))
	float Seconds = 5.f;
};

/**
 * 화로·모닥불. 칸 3개(연료/재료/결과)가 고정된 보관함 + 시간 가공 + 불빛.
 * 등급(모닥불/화로/상위 화로)은 BP 자식에서 Recipes · FuelSeconds · FireLight 만 바꾼다.
 * 불은 플레이어가 켜고, 가공할 게 없거나(재료 없음·결과 칸 자리 없음) 연료가 다하면 자동으로 꺼진다.
 */
UCLASS()
class FALLENERA_API AFEBuildFurnace : public AFEBuildStorage
{
	GENERATED_BODY()

public:
	AFEBuildFurnace();

	static constexpr int32 FuelSlot = 0;
	static constexpr int32 InputSlot = 1;
	static constexpr int32 OutputSlot = 2;
	static constexpr int32 SlotNum = 3;

	/** [Server Only] 불 켜기/끄기. 켤 때는 CanIgnite 를 다시 검사한다 */
	void SetLit(bool bNewLit);

	/** 지금 불을 붙일 수 있는가. 서버·클라 공용 (리플리케이트된 칸과 BP 기본값만 본다) */
	bool CanIgnite(FText& OutReason) const;

	UFUNCTION(BlueprintPure, Category = "FallenEra|Building|Furnace")
	bool IsLit() const;

	/** 지금 가공 중인 재료의 진행도 0~100 */
	UFUNCTION(BlueprintPure, Category = "FallenEra|Building|Furnace")
	uint8 GetProcessPercent() const;

	/** [Native] 점화·진행도 변화. 서버·클라 모두 */
	FSimpleMulticastDelegate OnFurnaceStateChangedNative;

	virtual int32 StoreItems(FGameplayTag ItemTag, int32 Count) override;
	virtual bool AcceptsItem(FGameplayTag ItemTag, FText& OutReason) const override;
	virtual FText GetSlotLabel(int32 SlotIndex) const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_IsLit)
	bool bIsLit = false;

	UPROPERTY(ReplicatedUsing = OnRep_ProcessPercent)
	uint8 ProcessPercent = 0;

	/** 재료 → 결과 표. ponytail: 제작 시스템이 레시피를 소유하게 되면 그쪽 조회로 교체 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Furnace")
	TArray<FFEFurnaceRecipe> Recipes;

	/** 연료 종류별 1개당 연소 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Furnace", meta = (Categories = "Item"))
	TMap<FGameplayTag, float> FuelSeconds;

	/** 불빛. 밝기·반경·색은 BP 자식의 컴포넌트 디테일에서 등급별로 조절 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Furnace")
	TObjectPtr<UPointLightComponent> FireLight;

	UFUNCTION()
	void OnRep_IsLit();

	UFUNCTION()
	void OnRep_ProcessPercent();

	/** BP 훅 (불 VFX/SFX). 서버·클라 모두 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallenEra|Building|Furnace")
	void OnLitChanged(bool bNewLit);

	virtual void ClearSlot(int32 SlotIndex) override;
	virtual FText GetInteractTextBuilt(AActor* InstigatorActor) const override;
	virtual void ReadRecord(const FFEBuildPieceRecord& Record) override;

public:
	/** [Server Only] 불이 켜져 있는 동안 0.5초 간격 */
	void TickFire();

	/** 재료 칸의 레시피. 재료가 없거나 결과 칸에 자리가 없으면 nullptr */
	const FFEFurnaceRecipe* GetActiveRecipe() const;

	/** [Server Only] 연료 칸에서 1개를 태워 FuelLeft 를 채운다. 연료가 없으면 false */
	bool ConsumeFuelUnit();

	/** 이 아이템이 들어갈 칸. 못 들어가면 INDEX_NONE. 연료이면서 재료인 아이템은 연료로 */
	int32 GetSlotForItem(FGameplayTag ItemTag) const;

	/** Contents 를 3칸으로 맞추고 새로 생긴 칸은 비운다 */
	void EnsureSlots();

	FTimerHandle FireTimer;

	/** [Server Only] 지금 타는 연료 1개의 남은 시간. 불이 꺼지면 버린다 */
	float FuelLeft = 0.f;

	/** [Server Only] 지금 재료 1개의 경과 시간. 불이 꺼지면 처음부터 */
	float ProcessElapsed = 0.f;
};
