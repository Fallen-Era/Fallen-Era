#pragma once

#include "NativeGameplayTags.h" 
	// 태그 이름이 다른 코드와 겹치지 않도록 고유한 이름공간(Namespace)으로 묶어줍니다.

namespace FallenEraItemTags
{
// ====================================================================
// 1. 역할 범주 (Category) - 이 아이템의 최종적인 '용도'는 무엇인가?
// ====================================================================
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon);      // 무기류
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon_Melee);         // 근거리 무기 그룹
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon_Melee_Sword);   // 검 (한손/양손검 등)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon_Melee_Axe);     // 도끼
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon_Melee_Spear);   // 창
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon_Ranged);        // 원거리 무기 그룹
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon_Ranged_Bow);    // 활
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Weapon_Ranged_Gun);    // 총기
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Armor);       // 방어구류
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Consumable);  // 소모품류
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Resource);    // 1차 채집 자원
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Material);    // 2차 가공 재료
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Category_Building);    // 건축물 (토대, 벽 등)

// ====================================================================
// 2. 재질 (Material) - 이 아이템은 '무엇으로 이루어져 있는가?'
// ====================================================================
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Material_Wood);             // 나무 소재
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Material_Stone);            // 돌 소재
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Material_Metal);            // 금속 소재
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Material_Plant);            // 식물 소재
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Material_Flesh);            // 고기/가죽 소재
	// TODO: 이 이상의 소재는 필요시 개발하며 Tag 추가 

// ====================================================================
// 3. 행동 및 특성 (Trait / Action) - 이 아이템으로 '무엇을 할 수 있는가?'
// ====================================================================
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_Flammable);           // 불에 탈 수 있음
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_Smeltable);           // 용광로에서 제련 가능
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_Cookable);            // 요리 가능

// 3-1. 행동 요구 조건 (채집/파괴 시 필요한 도구)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Requires_Axe);       // 도끼질 필요
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Requires_Pickaxe);   // 곡괭이질 필요
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Requires_Sickle);    // 낫질 필요

// ====================================================================
// 4. 아이템 스탯 (Stat) - 모디파이어(Modifier) 및 수치 연산의 타겟
// ====================================================================

// 4-1. 공격 관련 수치 (Offense)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_PhysicalAttack);       // 물리 공격력
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_AttackSpeed);          // 공격 속도
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_CritChance);           // 치명타 확률
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Offense_CritDamage);           // 치명타 피해량

// 4-2. 방어 관련 수치 (Defense)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Defense_PhysicalArmor);        // 물리 방어력
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Defense_Evasion);              // 회피율
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Defense_DamageReduction);      // 피해 감소율

// 4-3. 보조 전투 수치 (Auxiliary)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Aux_Accuracy);                 // 명중률
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Aux_Range);                    // 사거리
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Aux_Penetration);              // 관통력

// 4-4. 특수 전투 효과 (Special Effect) - 상태이상 및 DoT
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Special_Chance_Stun);          // 상태이상 부여 확률: 기절
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Special_Chance_Bleed);         // 상태이상 부여 확률: 출혈
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Special_Chance_Poison);        // 상태이상 부여 확률: 중독
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Special_DamageOverTime);       // 연속 피해 (DoT) 위력

// 4-5. 내구도 및 무게 (Durability & Weight)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Durability_Max);               // 최대 내구도
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Durability_DecreaseRate);      // 내구도 감소 배율 (전투/채집 시 닳는 속도 조절)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Weight_Value);                 // 아이템 개별 무게
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Weight_CapacityBonus);         // 인벤토리 최대 무게 한도 보너스 (가방류 아이템 장착 시)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Repair_CostMultiplier);        // 수리 비용/재료 배율 (패널티나 특성 연동용)
}

