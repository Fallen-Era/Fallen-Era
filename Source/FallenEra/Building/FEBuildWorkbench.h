// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildPiece.h"
#include "FEBuildWorkbench.generated.h"

/** 작업대. E 로 제작 진입. 제작 UI 는 아이템 담당 시스템이 BP 이벤트(OnWorkbenchUsedLocal)에 연결한다. */
UCLASS()
class FALLENERA_API AFEBuildWorkbench : public AFEBuildPiece
{
	GENERATED_BODY()

protected:
	virtual bool CanInteractBuilt(AActor* InstigatorActor) const override;
	virtual FText GetInteractTextBuilt(AActor* InstigatorActor) const override;
	virtual void InteractBuilt(AActor* InstigatorActor) override;
	virtual void InteractBuiltLocal(AActor* InstigatorActor) override;

	/** [Server Only] BP 훅 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallenEra|Building|Workbench")
	void OnWorkbenchUsed(AActor* InstigatorActor);

	/** [Client Only] 요청한 클라이언트에서 호출. 여기서 제작 UI 를 연다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallenEra|Building|Workbench")
	void OnWorkbenchUsedLocal(AActor* InstigatorActor);
};
