// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FEBuildingSettings.generated.h"

class UMaterialInterface;

/** Project Settings > Game > Building. 모든 피스가 공유하는 튜닝 값. DefaultGame.ini 에 저장되어 팀과 공유됨. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Building"))
class FALLENERA_API UFEBuildingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFEBuildingSettings();

	static const UFEBuildingSettings* Get();

	/** 한 바퀴(360도)를 RotationStepDeg 로 나눈 스텝 수. 회전을 uint8 로 패킹할 때 클라/서버가 같은 값을 써야 함. */
	int32 GetYawStepCount() const;

	/** 회전 입력 1회당 Yaw. 회의 결정: 15도 */
	UPROPERTY(config, EditAnywhere, Category = "FallenEra|Building|Placement", meta = (ClampMin = 1, ClampMax = 90))
	float RotationStepDeg = 15.f;

	/** 카메라에서 이 거리(cm) 안에만 배치 가능 */
	UPROPERTY(config, EditAnywhere, Category = "FallenEra|Building|Placement", meta = (ClampMin = 100))
	float MaxBuildDistance = 800.f;

	/** 프리뷰 주변 이 반경(cm) 안에서 스냅 소켓 탐색. S3 부터 사용. */
	UPROPERTY(config, EditAnywhere, Category = "FallenEra|Building|Placement", meta = (ClampMin = 10))
	float SnapRadius = 150.f;

	UPROPERTY(config, EditAnywhere, Category = "FallenEra|Building|Visual")
	TSoftObjectPtr<UMaterialInterface> GhostValidMaterial;

	UPROPERTY(config, EditAnywhere, Category = "FallenEra|Building|Visual")
	TSoftObjectPtr<UMaterialInterface> GhostInvalidMaterial;

	UPROPERTY(config, EditAnywhere, Category = "FallenEra|Building|Visual")
	TSoftObjectPtr<UMaterialInterface> BlueprintMaterial;

	/** 샌드박스 옵션 기본값: 구조물이 데미지를 받는가. S7 부터 사용. */
	UPROPERTY(config, EditAnywhere, Category = "FallenEra|Building|Sandbox")
	bool bStructureDamageEnabled = true;
};
