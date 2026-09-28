// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FEBuildingTypes.h"
#include "UObject/Interface.h"
#include "FEBuildInventoryProvider.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UFEBuildInventoryProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * 건설 시스템이 인벤토리에 요구하는 최소 경계.
 * 아이템 담당의 실제 인벤토리 컴포넌트가 이 인터페이스를 구현하면 건설 코드는 수정 없이 연결된다.
 * 모든 함수는 [Server Only] 로 호출된다.
 */
class FALLENERA_API IFEBuildInventoryProvider
{
	GENERATED_BODY()

public:
	/** 보유 수량 */
	virtual int32 CountItems(FGameplayTag ItemTag) const = 0;

	/** 최대 Count 만큼 제거하고 실제 제거된 수량을 돌려준다 */
	virtual int32 RemoveItems(FGameplayTag ItemTag, int32 Count) = 0;

	/** 최대 Count 만큼 추가하고 실제로 추가된 수를 돌려준다 (인벤토리가 가득 차면 Count 보다 작다) */
	virtual int32 AddItems(FGameplayTag ItemTag, int32 Count) = 0;
	
	/** 보유 중인 모든 아이템 (개수 0 은 제외). 저장고 패널이 넣을 수 있는 목록을 만들 때 쓴다 */
	virtual void GetItems(TArray<FFEBuildItemCost>& OutItems) const = 0;
};