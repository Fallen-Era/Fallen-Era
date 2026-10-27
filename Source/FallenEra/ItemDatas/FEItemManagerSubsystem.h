// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "FEItemManagerSubsystem.generated.h"

class UFEItemBase;

DECLARE_LOG_CATEGORY_EXTERN(LogFEItemManager, Log, All);

/**
 * UFEItemManagerSubsystem
 * - GameInstance 생명주기 동안 프로젝트 내의 모든 UFEItemBase Data Asset을 캐싱합니다.
 * - 런타임 동적 데이터(FItemInstance)가 가진 ItemID(FName)를 기반으로 정적 데이터(const UFEItemBase*)를 O(1)로 고속 조회합니다.
 */
UCLASS()
class FALLENERA_API UFEItemManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFEItemManagerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** ItemID로 원본 ItemBase 데이터 에셋 검색 (미등록 시 nullptr) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FallenEra|Item")
	const UFEItemBase* GetItemData(FName InItemID) const;

	/** FPrimaryAssetId로 원본 ItemBase 데이터 에셋 검색 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FallenEra|Item")
	const UFEItemBase* GetItemDataByPrimaryId(FPrimaryAssetId InAssetId) const;

	/** ItemID로 특정 클래스 타입(예: UFEWeaponItem, UFEEquipmentItem)의 에셋 검색 (C++ 전용) */
	template<typename T>
	const T* GetItemData(FName InItemID) const
	{
		return Cast<T>(GetItemData(InItemID));
	}

	/** FPrimaryAssetId로 특정 클래스 타입의 에셋 검색 (C++ 전용) */
	template<typename T>
	const T* GetItemDataByPrimaryId(FPrimaryAssetId InAssetId) const
	{
		return Cast<T>(GetItemDataByPrimaryId(InAssetId));
	}

	/** 해당 ItemID가 등록되어 있는지 여부 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FallenEra|Item")
	bool HasItemData(FName InItemID) const;

	/** 캐시된 모든 아이템 데이터 반환 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FallenEra|Item")
	TArray<UFEItemBase*> GetAllItemData() const;

	/** 특정 GameplayTag를 가진 아이템 목록 검색 (카테고리, 특성 필터링용) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "FallenEra|Item")
	TArray<UFEItemBase*> GetItemsByTag(FGameplayTag MatchingTag) const;

	/** 아이템 캐시 수동 재구축 */
	UFUNCTION(BlueprintCallable, Category = "FallenEra|Item")
	void RefreshItemCache();

private:
	/** ItemID -> UFEItemBase* 캐시 사전 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UFEItemBase>> ItemCache;
};
