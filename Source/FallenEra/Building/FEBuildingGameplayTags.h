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
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Socket_Furniture_Base);

    // ponytail: 아이템 담당(병일)이 Item.* 태그를 정의하면 이 줄과 DA 의 재료 태그를 교체하고 삭제
    FALLENERA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Resource_Wood);
}