#pragma once

#include "CoreMinimal.h"
#include "FallenEraItemType.generated.h"

/**
 * Runtime 동기화 데이터 구조화 
 *  1. 장착 슬롯 Enum 정의
 *  2. 동적 데이터(런타임 인스턴스) 구조체 정의
 */

// ----------------------------------------------------
// 1. 장착 슬롯 Enum 정의
// ----------------------------------------------------
UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
    None        UMETA(DisplayName = "None"),
    MainHand    UMETA(DisplayName = "Main Hand"),
    OffHand     UMETA(DisplayName = "Off Hand"),
    TwoHand     UMETA(DisplayName = "Two Handed"),
    Head        UMETA(DisplayName = "Head"),
    Chest       UMETA(DisplayName = "Chest"),
    Legs        UMETA(DisplayName = "Legs"),
    Feet        UMETA(DisplayName = "Feet")
};

// ----------------------------------------------------
// 2. 동적 데이터(런타임 인스턴스) 구조체 정의
// ----------------------------------------------------
USTRUCT(BlueprintType)
struct FItemInstance
{
    GENERATED_BODY() 

public:
    // 고유 식별자 (네트워크 동기화 및 DB 저장 시 '이 아이템'을 정확히 식별)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid InstanceID; 

    // 소유자 식별 (누가 이 아이템의 주인인가?)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OwnerID;

    // 원본 아이템(Data Asset)을 가리키는 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ItemID; 

    // 중첩 가능한 아이템(자원, 소비품) 전용 수량
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StackCount;

    // 장비를 위한 현재 내구도
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentDurability;

    // 건축물 배치 시 인스턴스화 될 때 사용하는 현재 체력
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentHP;

    // 장비 전용 랜덤 특성 부여용 시드값 (동일 아이템도 다른 특성 부여)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TraitSeed;

    // 생성자 (기본값 초기화)
    FItemInstance()
    {
        InstanceID = FGuid::NewGuid(); // 생성 시 무조건 겹치지 않는 고유 ID 발급
        OwnerID = TEXT("");
        ItemID = TEXT("");
        StackCount = 1;
        CurrentDurability = 100.0f;
        CurrentHP = 100.0f;
        TraitSeed = 0;
    }
};