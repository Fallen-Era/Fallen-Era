// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Building/FEBuildInventoryProvider.h"
#include "FEMockInventoryComponent.generated.h"

/**
 * ponytail: 아이템 담당의 인벤토리가 오기 전까지 쓰는 임시 재고.
 * 실제 인벤토리가 도착하면 이 파일을 삭제하고, 그 컴포넌트가 IFEBuildInventoryProvider 를 구현한다.
 * 서버 값만 의미가 있으며 리플리케이트하지 않는다 (UI 없음).
 */
UCLASS(ClassGroup = (FallenEra), meta = (BlueprintSpawnableComponent))
class FALLENERA_API UFEMockInventoryComponent : public UActorComponent, public IFEBuildInventoryProvider
{
	GENERATED_BODY()

public:
	virtual int32 CountItems(FGameplayTag ItemTag) const override;
	virtual int32 RemoveItems(FGameplayTag ItemTag, int32 Count) override;
	virtual void AddItems(FGameplayTag ItemTag, int32 Count) override;

protected:
	/** 테스트용 초기 재고. 예: Item.Resource.Wood = 100 */
	UPROPERTY(EditAnywhere, Category = "FallenEra|Building|Mock", meta = (Categories = "Item"))
	TMap<FGameplayTag, int32> Items;
};
