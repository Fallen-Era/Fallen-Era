// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildPiece.h"
#include "FEBuildBed.generated.h"

class APlayerState;

/** 침구. E 로 자기 리스폰 지점으로 지정. 플레이어당 하나, 침구당 주인 하나. 리스폰 위치는 UFEBuildingSubsystem::GetRespawnTransform. */
UCLASS()
class FALLENERA_API AFEBuildBed : public AFEBuildPiece
{
	GENERATED_BODY()

public:
	/** [Server Only] 서브시스템이 호출. 빈 문자열이면 주인 없음 */
	void SetOwnerPlayerId(const FString& NewOwnerId);

	const FString& GetOwnerPlayerId() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 이 침구를 리스폰 지점으로 쓰는 플레이어의 안정 ID. 프롬프트 문구용으로 리플리케이트 */
	UPROPERTY(Replicated)
	FString OwnerPlayerId;

	virtual bool CanInteractBuilt(AActor* InstigatorActor) const override;
	virtual FText GetInteractTextBuilt(AActor* InstigatorActor) const override;
	virtual void InteractBuilt(AActor* InstigatorActor) override;
	virtual void WriteRecord(FFEBuildPieceRecord& OutRecord) const override;
	virtual void ReadRecord(const FFEBuildPieceRecord& Record) override;

private:
	static APlayerState* GetPlayerStateOf(const AActor* Actor);
};
