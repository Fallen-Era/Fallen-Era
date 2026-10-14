#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FEItemType.generated.h"

/**
 * Runtime 동기화 및 제작 연동 데이터 구조화 
 *  1. 장착 슬롯 Enum 정의
 *  2. 연산자 Enum 및 크래프팅 기여도 구조체 정의
 *  3. 동적 데이터(런타임 인스턴스) 구조체 정의
 */

// ----------------------------------------------------
// 1. 장착 슬롯 Enum 정의 (물리적 신체 착용 부위)
// ----------------------------------------------------
UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
    None        UMETA(DisplayName = "None"),
    MainHand    UMETA(DisplayName = "Main Hand"),
    OffHand     UMETA(DisplayName = "Off Hand"),
    Head        UMETA(DisplayName = "Head"),
    Chest       UMETA(DisplayName = "Chest"),
    Legs        UMETA(DisplayName = "Legs"),
    Feet        UMETA(DisplayName = "Feet")
};

// ----------------------------------------------------
// 2. 연산자 및 크래프팅 기여도 구조체 정의
// ----------------------------------------------------
UENUM(BlueprintType)
enum class EModifierOp : uint8
{
    Add         UMETA(DisplayName = "Add (+)"),
    Multiply    UMETA(DisplayName = "Multiply (*)")
};

USTRUCT(BlueprintType)
struct FALLENERA_API FCraftingModifier
{
    GENERATED_BODY()

    /** 어떤 카테고리의 제작 결과물에 적용할지 (예: Item.Category.Weapon, Item.Category.Armor) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (Categories = "Item.Category"))
    FGameplayTag TargetCategoryTag;

    /** 변경할 스탯 태그 (예: Stat.Offense.PhysicalAttack, Stat.Durability.Max) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (Categories = "Stat"))
    FGameplayTag TargetStatTag;

    /** 연산 방식 (더하기 or 곱하기) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    EModifierOp Operation = EModifierOp::Add;

    /** 적용 수치 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
    float Value = 0.0f;
};

// ----------------------------------------------------
// 3. 동적 데이터(런타임 인스턴스) 구조체 정의
// ----------------------------------------------------
USTRUCT(BlueprintType)
struct FALLENERA_API FItemInstance
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
    FName ItemID; 

    // 중첩 가능한 아이템(자원, 소비품) 전용 수량
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StackCount;

    // 장비를 위한 현재 내구도 (사용 시 소모)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentDurability;

    // 아이템 자체의 현재 체력 (필드 드랍 피격, 화염/폭발 등 환경 피해)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentHP;

    // 장비 전용 랜덤 특성 부여용 시드값 (동일 아이템도 다른 특성 부여)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TraitSeed;

    // 기본 생성자 (임시/빈 슬롯 초기화용: 무효 상태로 시작)
    FItemInstance()
        : InstanceID()
        , OwnerID(TEXT(""))
        , ItemID(NAME_None)
        , StackCount(0)
        , CurrentDurability(0.0f)
        , CurrentHP(0.0f)
        , TraitSeed(0)
    {
    }

    /** 서버에서 유효한 아이템 인스턴스를 신규 생성할 때 사용하는 팩토리 함수 */
    static FItemInstance CreateInstance(FName InItemID, int32 InStackCount = 1, float InDurability = 100.0f, float InHP = 100.0f, const FString& InOwnerID = TEXT(""))
    {
        FItemInstance NewInst;
        NewInst.InstanceID = FGuid::NewGuid();
        NewInst.ItemID = InItemID;
        NewInst.StackCount = InStackCount;
        NewInst.CurrentDurability = InDurability;
        NewInst.CurrentHP = InHP;
        NewInst.OwnerID = InOwnerID;
        return NewInst;
    }

    /** 유효한 아이템 인스턴스인지 검사 */
    bool IsValid() const
    {
        return InstanceID.IsValid() && !ItemID.IsNone() && StackCount > 0;
    }
};