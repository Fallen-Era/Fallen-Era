// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildDoor.h"
#include "FEBuildingSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

AFEBuildDoor::AFEBuildDoor()
{
    Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
    Panel->SetupAttachment(Mesh);
    Panel->SetCollisionProfileName(TEXT("BuildBlueprint"));
    // 문짝은 여닫히며 움직이므로 Movable. Static 이면 런타임 회전 시 NavMesh 가 갱신되지 않는다.
    Panel->SetMobility(EComponentMobility::Movable);
}

void AFEBuildDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AFEBuildDoor, bIsOpen);
}

bool AFEBuildDoor::IsOpen() const
{
    return bIsOpen;
}

void AFEBuildDoor::SetPreviewValid(bool bIsValid)
{
    Super::SetPreviewValid(bIsValid);
    const UFEBuildingSettings* Settings = UFEBuildingSettings::Get();
    ApplyMaterialToAll(Panel, bIsValid ? Settings->GhostValidMaterial : Settings->GhostInvalidMaterial);
}

void AFEBuildDoor::OnRep_IsOpen()
{
    Panel->SetRelativeRotation(FRotator(0.f, bIsOpen ? OpenYaw : 0.f, 0.f));
}

bool AFEBuildDoor::CanInteractBuilt(AActor* InstigatorActor) const
{
    return true;
}

FText AFEBuildDoor::GetInteractTextBuilt(AActor* InstigatorActor) const
{
    return bIsOpen ? LOCTEXT("CloseDoor", "문 닫기") : LOCTEXT("OpenDoor", "문 열기");
}

void AFEBuildDoor::InteractBuilt(AActor* InstigatorActor)
{
    bIsOpen = !bIsOpen;
    UE_LOG(LogFEBuilding, Log, TEXT("%s door %s"), *GetName(), bIsOpen ? TEXT("opened") : TEXT("closed"));
    
    OnRep_IsOpen(); // 서버 로컬에도 즉시 반영
}

void AFEBuildDoor::OnApplyState(EFEBuildPieceState NewState)
{
    // ponytail: 문짝은 정의 번들에 없어 여기서 동기 로드. 프로토 메시라 부담 없음. 실제 에셋에서 크면 비동기로.
    if (UStaticMesh* LoadedPanel = PanelMesh.LoadSynchronous())
    {
        Panel->SetStaticMesh(LoadedPanel);
    }
    Panel->SetRelativeLocation(HingeOffset);
    Panel->EmptyOverrideMaterials();
    Panel->SetCollisionProfileName(GetCollisionProfileForState(NewState));
    // 완성된 문만 NavMesh 에 반영. 청사진·고스트는 좀비가 그냥 지나간다.
    Panel->SetCanEverAffectNavigation(NewState == EFEBuildPieceState::Built);

    switch (NewState)
    {
    case EFEBuildPieceState::Preview:
        ApplyMaterialToAll(Panel, UFEBuildingSettings::Get()->GhostValidMaterial);
        break;
    case EFEBuildPieceState::Blueprint:
        ApplyMaterialToAll(Panel, UFEBuildingSettings::Get()->BlueprintMaterial);
        break;
    default:
        break;
    }
    OnRep_IsOpen();
}

void AFEBuildDoor::WriteRecord(FFEBuildPieceRecord& OutRecord) const
{
    Super::WriteRecord(OutRecord);
    OutRecord.Flags = bIsOpen ? 1 : 0;
}

void AFEBuildDoor::ReadRecord(const FFEBuildPieceRecord& Record)
{
    Super::ReadRecord(Record);
    bIsOpen = (Record.Flags & 1) != 0;
    OnRep_IsOpen(); // InitializePiece 가 닫힌 상태로 그려 놓았으므로 여기서 각도를 다시 잡는다
}

#undef LOCTEXT_NAMESPACE
