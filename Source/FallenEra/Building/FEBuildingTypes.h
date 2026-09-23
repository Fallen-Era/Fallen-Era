// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Logging/LogMacros.h"
#include "FEBuildingTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFEBuilding, Log, All);

/** 건설 피스 전용 오브젝트 채널. DefaultEngine.ini 에 "BuildPiece" 로 선언되어 있음. */
#define ECC_FEBuildPiece ECC_GameTraceChannel2

UENUM(BlueprintType)
enum class EFEBuildMaterial : uint8
{
	Wood,
	Stone,
	Metal
};

UENUM(BlueprintType)
enum class EFEBuildPieceState : uint8
{
	/** 배치 위치를 고르는 동안의 로컬 고스트. 절대 리플리케이트되지 않음. */
	Preview,
	/** 배치됐지만 재료가 아직 다 안 찬 상태. Overlap 전용 콜리전, 체력 없음. */
	Blueprint,
	/** 완성. 블로킹 콜리전, 체력 있음, 구조 지지 그래프에 참여. */
	Built
};

/** 재료 요구 1건: 아이템 ID(아이템 시스템 소유) + 개수 */
USTRUCT(BlueprintType)
struct FALLENERA_API FFEBuildItemCost
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (Categories = "Item"))
	FGameplayTag ItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (ClampMin = 1))
	int32 Count = 1;
};

/**
 * 피스에 붙은 스냅 지점. 로컬 X축이 바깥(상대 피스) 방향을 가리킨다.
 * 프리뷰 소켓 A 를 대상 소켓 B 에 붙일 때: PieceWorld = A.Local⁻¹ × Flip180 × B.World
 */
USTRUCT(BlueprintType)
struct FALLENERA_API FFEBuildSocket
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (Categories = "Build.Socket"))
	FGameplayTag Type;

	/** 이 소켓이 "붙는 쪽"일 때 허용하는 상대 소켓 종류. 비어 있으면 상대 전용 소켓(남이 붙는 자리). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building", meta = (Categories = "Build.Socket"))
	FGameplayTagContainer AcceptTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building")
	FTransform LocalTransform;
};
