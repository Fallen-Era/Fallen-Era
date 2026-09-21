// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildingGameplayTags.h"

namespace FEBuildingTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Build_Toggle,      "Ability.Input.Build.Toggle",    "B. 빌드 모드 진입/해제");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Build_RotateCW,    "Ability.Input.Build.RotateCW",  "E (빌드 모드 중)");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Build_RotateCCW,   "Ability.Input.Build.RotateCCW", "Q (빌드 모드 중)");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Build_Place,       "Ability.Input.Build.Place",     "LMB");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Build_Cancel,      "Ability.Input.Build.Cancel",    "RMB");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Build_Demolish,    "Ability.Input.Build.Demolish",  "X. 조준 피스 철거");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Build_Menu,        "Ability.Input.Build.Menu",      "Tab. 빌드 메뉴 열기/닫기 (빌드 모드 밖에서 누르면 진입)");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Interact,          "Ability.Input.Interact",        "E. 조준/근접 IFEInteractable 과 상호작용. 팀 공용 후보");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Piece_Structure,           "Build.Piece.Structure",  "토대, 벽, 천장, 기둥, 문");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Piece_Functional,          "Build.Piece.Functional", "작업대, 저장고, 화로, 모닥불, 침구");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Piece_Defense,             "Build.Piece.Defense",    "스파이크, 철조망, 발리스타");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Piece_Transport,           "Build.Piece.Transport",  "텔레포터");

    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Foundation_Edge,            "Build.Socket.Foundation.Edge");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Foundation_Top,             "Build.Socket.Foundation.Top");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Foundation_Corner,  "Build.Socket.Foundation.Corner", "기둥이 서는 자리");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Wall_Bottom,                "Build.Socket.Wall.Bottom");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Wall_Top,                   "Build.Socket.Wall.Top");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Wall_Side,                  "Build.Socket.Wall.Side");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Pillar_Base,                "Build.Socket.Pillar.Base");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Pillar_Top,                 "Build.Socket.Pillar.Top");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Ceiling_Edge,               "Build.Socket.Ceiling.Edge");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Ceiling_Top,                "Build.Socket.Ceiling.Top");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Ceiling_Corner,     "Build.Socket.Ceiling.Corner", "Pillar.Top 위에 얹힘 / 2층 기둥이 서는 자리");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Ceiling_CornerTop,  "Build.Socket.Ceiling.CornerTop", "천장 윗면 코너. 2층 기둥이 서는 자리 (상대 전용)");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Furniture_Base,             "Build.Socket.Furniture.Base");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Resource_Wood,              "Item.Resource.Wood", "TEMP (KJH). 아이템 담당이 Item.* 를 정의하면 제거");
}