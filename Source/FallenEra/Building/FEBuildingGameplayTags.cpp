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
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Ballista_Exit,     "Ability.Input.Ballista.Exit",   "E·우클릭 (발리스타 탑승 중). 내리기");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Ballista_Fire,     "Ability.Input.Ballista.Fire",   "좌클릭 (발리스타 탑승 중). 발사");
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Building,                  "State.Building",                "빌드 모드 중 (로컬). 상호작용 등이 이 태그로 스스로 꺼진다");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Piece_Structure,           "Build.Piece.Structure",  "토대, 벽, 천장, 기둥, 문, 계단, 난간, 창문 벽, 지붕, 삼각 벽");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Piece_Functional,          "Build.Piece.Functional", "작업대, 저장고, 화로, 모닥불, 침구");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Piece_Defense,             "Build.Piece.Defense",    "스파이크, 철조망, 발리스타");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Piece_Transport,           "Build.Piece.Transport",  "텔레포터");

    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Foundation_Edge,            "Build.Socket.Foundation.Edge");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Foundation_Top,             "Build.Socket.Foundation.Top");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Foundation_Corner,  "Build.Socket.Foundation.Corner",   "기둥이 서는 자리");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Wall_Bottom,                "Build.Socket.Wall.Bottom");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Wall_Top,                   "Build.Socket.Wall.Top");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Wall_Side,                  "Build.Socket.Wall.Side");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Pillar_Base,                "Build.Socket.Pillar.Base");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Pillar_Top,                 "Build.Socket.Pillar.Top");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Ceiling_Edge,               "Build.Socket.Ceiling.Edge");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Ceiling_Top,                "Build.Socket.Ceiling.Top");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Ceiling_Corner,     "Build.Socket.Ceiling.Corner",      "Pillar.Top 위에 얹힘 / 2층 기둥이 서는 자리");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Ceiling_CornerTop,  "Build.Socket.Ceiling.CornerTop",   "천장 윗면 코너. 2층 기둥이 서는 자리 (상대 전용)");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Ceiling_Seat,       "Build.Socket.Ceiling.Seat",        "벽 윗면(Wall.Top)에 얹히는 자리. 벽 중심선이 토대 가장자리에서 10 안쪽이라 Edge(±200)가 아니라 ±190");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Roof_Bottom,        "Build.Socket.Roof.Bottom",         "지붕 낮은 쪽 가장자리. Wall.Top 또는 아래 지붕의 Roof.Top 에 붙는다");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Build_Socket_Roof_Top,           "Build.Socket.Roof.Top",            "지붕 용마루. 위로 이어지는 지붕이 붙고, 반대편 지붕의 용마루와 마주 보면 연결된다");
    UE_DEFINE_GAMEPLAY_TAG(Build_Socket_Furniture_Base,             "Build.Socket.Furniture.Base");

    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Resource_Wood,              "Item.Resource.Wood",       "TEMP (KJH). 아이템 담당이 Item.* 를 정의하면 제거");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Resource_Stone,             "Item.Resource.Stone",      "TEMP (KJH). 아이템 담당이 Item.* 를 정의하면 제거");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Resource_IronOre,           "Item.Resource.IronOre",    "TEMP (KJH). 아이템 담당이 Item.* 를 정의하면 제거");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Resource_IronIngot,         "Item.Resource.IronIngot",  "TEMP (KJH). 아이템 담당이 Item.* 를 정의하면 제거");
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Food_RawMeat,               "Item.Food.RawMeat",        "TEMP (KJH). 아이템 담당이 Item.* 를 정의하면 제거");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Food_CookedMeat,            "Item.Food.CookedMeat",     "TEMP (KJH). 아이템 담당이 Item.* 를 정의하면 제거");
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Resource_MetalScrap,        "Item.Resource.MetalScrap", "TEMP (KJH). 철조망 재료. 아이템 담당이 Item.* 를 정의하면 제거");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Resource_Rope,              "Item.Resource.Rope",       "TEMP (KJH). 발리스타 재료. 아이템 담당이 Item.* 를 정의하면 제거");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo_Bolt,                  "Item.Ammo.Bolt",           "TEMP (KJH). 발리스타 탄약. 아이템 담당이 Item.* 를 정의하면 제거");
    
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Tool_Hammer,                "Item.Tool.Hammer", "수리 도구 (임시. 아이템 시스템이 오면 교체)");
}