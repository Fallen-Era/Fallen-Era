#pragma once

#include "CoreMinimal.h"
#include "WorldSystemData.generated.h"


USTRUCT(Blueprintable)
struct FRegionSeed
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Center = FVector::ZeroVector;
	
};


USTRUCT(Blueprintable)
struct FRegionEdge
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int	RegionIdx;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector A;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector B;
};

/*
 * 각 섹터가 가질 수 있는 여러 길 형태의 정의.
 */
UENUM(Blueprintable)
enum class EWayType : uint8
{
	Paved,		// 아스팔트, 콘크리트 등 포장 도로
	Unpaved,	// 흙/자갈 등으로 도로 형태는 조성됨
	Trail,		// 자연 지형을 따라 최소한으로 형성된 길
	OffRoad,	// 실제 길 없음, 지형 그대로 통과
};


/**
 * 주변 환경 Tag
 * Prefab이 같은 Tag로 각 Prefab은 이 Tag에 지정된 가중치 연산에 따라
 * 그 Sector에 배치될 확률을 지정 받는다.
 */
UENUM(Blueprintable)
enum class EEnvironmentTag : uint8
{
	None,
	Urban,			// 도심 : 상업지구, 오피스, 주거단지, 지하도, 지하철
	Suburban,		// 교외 : 소도시, 마을, 주택가, 휴게소, 주유소, 소형 마트
	Rural,			// 농촌, 목장, 과수원, 곡물창고, 농기게 창고, 작은 마을
	Industrial,		// 산업 / 물류 : 공장, 제철소, 정유시설, 물류센터, 창고, 폐차장, 건설현장
	Research,		// 연구 : 대학교, 연구단지, 생명공학 연구소, 제약회사, 데이터센터, 실험시설
	Military,		// 군사 : 경찰서, 군부대, 훈련장, 탄약고, 공군기지, 방공기지, 지하 벙커, 해군 기지, 전략 지휘시설
	Medical,		// 의료 : 종합병원, 약국, 연구병원, 혈액원
	Government,		// 정부 : 시청 경찰서, 법원, 교도소
	Transportation,	// 교통 : 고속도로, 터널, 휴게소, 철도, 기차역, 지하철, 공항
	Energy,			// 에너지 : 원자력 발전소, 화력 발전소, 수력 발전소, 변전소
	Harbor,			// 항만 : 항구, 조선소, 부두, 선박, 창고, 해경시설
	Wilderness,		// 자연 : 산악, 숲, 습지, 국립 공원, 동굴, 계곡, 야영지
	Special			// 특수구역 : 교도소, 방공호, 지하시설, 비밀 연구소
};




/*
 * 보르노이 다이어그램에 사용할 바이옴 구성
 */
UENUM(Blueprintable)
enum class EBiomeType : uint8
{
	TemperateForest,	// 온대 산림
	Plains,				// 평원
	Hills,				// 산악
	Desert,				// 사막
	Tundra,				// 극지방
};


/*
 * 초기 단계 구현에서 미사용.
 */
UENUM(Blueprintable)
enum class EBiomeEnvironmentOverlay : uint8
{
	None,
	Radioactive,		// 방사능
	Biological,			// 생물학적 오염
	Infected,			// 감염
	Chemical,			// 화학적 오염
};



/*
 * Sector 내부에 배치되는 Prefab의 정의.
 */
USTRUCT(Blueprintable)
struct FPOI_Prefab
{
	
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEnvironmentTag RegionType = EEnvironmentTag::None;
	
};

USTRUCT(Blueprintable)
struct FWayPoint
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EWayType WayType = EWayType::OffRoad;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, 
		meta = (EditCondition = "WayType == EWayType::Paved || WayType == EWayType::Unpaved"))
	bool Tunnel = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, 
		meta = (EditCondition = "WayType == EWayType::Paved || WayType == EWayType::Unpaved"))
	bool Bridge = false;
};


USTRUCT(Blueprintable)
struct FWorldSectorDefinition
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWayPoint> WayPoints;
	
	
	
};





