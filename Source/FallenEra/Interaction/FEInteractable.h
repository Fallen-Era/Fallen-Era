// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FEInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UFEInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * E 키로 상호작용할 수 있는 액터. C++ 와 BP 모두 구현 가능.
 * 호출은 반드시 IFEInteractable::Execute_XXX(Target, InstigatorActor) 로 (BP 구현체도 동작).
 * 건설 피스(청사진 재료 투입, 문, 작업대, 침구)와 아이템 픽업이 같은 인터페이스를 쓴다.
 */
class FALLENERA_API IFEInteractable
{
	GENERATED_BODY()

public:
	/** 클라/서버 공용. 타게팅 후보 필터와 프롬프트 표시 여부 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallenEra|Interaction")
	bool CanInteract(AActor* InstigatorActor) const;

	/** 프롬프트 문구. 예: "문 열기", "재료 투입 (Item.Resource.Wood 3개)" */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallenEra|Interaction")
	FText GetInteractText(AActor* InstigatorActor) const;

	/** [Server Only] 상태 변경. UFEInteractionComponent 가 거리와 CanInteract 를 재검증한 뒤 호출 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallenEra|Interaction")
	void Interact(AActor* InstigatorActor);

	/** [Client Only] 서버 Interact 성공 후 요청한 클라이언트에서 호출. UI 열기·로컬 연출용 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallenEra|Interaction")
	void InteractLocal(AActor* InstigatorActor);
};
