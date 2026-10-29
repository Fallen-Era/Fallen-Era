// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

/**
 * 건설·상호작용 태그.
 * 모듈 로드 시 등록되므로 config 로드 순서와 무관하며, 오타는 컴파일 에러로 잡힌다.
 */
namespace FEBuildingTags
{
    // 입력 (GA_Build_* / GA_Interact 의 Ability Input Bindings 에서 사용)
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Build_Toggle);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Build_RotateCW);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Build_RotateCCW);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Build_Place);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Build_Cancel);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Build_Demolish);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Build_Menu);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Interact);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Ballista_Exit);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Ballista_Fire);
    
    // 상태 (로컬 loose 태그. 다른 시스템이 "빌드 중"을 알아야 할 때 이 태그만 본다)
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Building);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Building_Mounted);

    // 피스 분류 (UFEBuildPieceDefinition::Category)
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Piece_Structure);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Piece_Functional);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Piece_Defense);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Piece_Transport);

    // 소켓 (FFEBuildSocket::Type / AcceptTypes)
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Foundation_Edge);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Foundation_Top);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Foundation_Corner);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Wall_Bottom);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Wall_Top);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Wall_Side);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Pillar_Base);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Pillar_Top);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Ceiling_Edge);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Ceiling_Top);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Ceiling_Corner);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Ceiling_CornerTop);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Ceiling_Seat);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Roof_Bottom);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Roof_Top);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Furniture_Base);

    // 아이템 담당이 Item.* 태그를 정의하면 이 줄과 DA 의 재료 태그를 교체하고 삭제
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Resource_Wood);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Resource_Stone);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Resource_IronOre);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Resource_IronIngot);
    
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Food_RawMeat);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Food_CookedMeat);
    
    // 방어시설 재료·탄약 (임시. 아이템 담당이 정하면 교체)
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Resource_MetalScrap);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Resource_Rope);
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo_Bolt);
    
    // 망치 아이템 식별 태그 (망치 DA Template > Item Tags, 장비 Test Item Tags 매칭용).
    // "들고 있는지" 판정은 HT 장착 태그 Equipment.Weapon.Hammer 로 한다 (FEBuildingComponent::IsHoldingRepairTool)
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Tool_Hammer);
}