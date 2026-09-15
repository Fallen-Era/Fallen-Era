// Fallen Era 건설 시스템 (KJH)

#include "Building/FEBuildPiece.h"
#include "Building/FEBuildPieceDefinition.h"
#include "Building/FEBuildingSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

namespace
{
    void SetAllMaterials(UStaticMeshComponent* MeshComponent, const TSoftObjectPtr<UMaterialInterface>& SoftMaterial)
    {
        // 고스트/청사진 머티리얼은 작은 Unlit 머티리얼이라 첫 사용 시 동기 로드해도 부담이 없다.
        UMaterialInterface* Material = SoftMaterial.LoadSynchronous();
        if (Material == nullptr)
        {
            return;
        }
        for (int32 SlotIndex = 0; SlotIndex < MeshComponent->GetNumMaterials(); ++SlotIndex)
        {
            MeshComponent->SetMaterial(SlotIndex, Material);
        }
    }
}

AFEBuildPiece::AFEBuildPiece()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BuildBlueprint"));
}

void AFEBuildPiece::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AFEBuildPiece, PieceId);
    DOREPLIFETIME(AFEBuildPiece, State);
}

void AFEBuildPiece::InitializePiece(const UFEBuildPieceDefinition* InDefinition, EFEBuildPieceState InState)
{
    Definition = InDefinition;
    PieceId = InDefinition ? InDefinition->GetPrimaryAssetId() : FPrimaryAssetId();
    State = InState;

    if (State == EFEBuildPieceState::Preview)
    {
        // 로컬 고스트. FinishSpawning 전에 호출해야 한다. 안 그러면 리슨 서버 호스트의 고스트가 다른 클라이언트에 리플리케이트된다.
        SetReplicates(false);
    }
    ApplyState();
}

void AFEBuildPiece::SetState(EFEBuildPieceState NewState)
{
    const bool bCanChange = HasAuthority() && State != NewState;
    if (!bCanChange)
    {
        return;
    }
    State = NewState;
    ApplyState();
}

void AFEBuildPiece::SetPreviewValid(bool bIsValid)
{
    const UFEBuildingSettings* Settings = UFEBuildingSettings::Get();
    SetAllMaterials(Mesh, bIsValid ? Settings->GhostValidMaterial : Settings->GhostInvalidMaterial);
}

const UFEBuildPieceDefinition* AFEBuildPiece::GetDefinition() const
{
    return Definition;
}

EFEBuildPieceState AFEBuildPiece::GetState() const
{
    return State;
}

UStaticMeshComponent* AFEBuildPiece::GetMesh() const
{
    return Mesh;
}

void AFEBuildPiece::OnRep_PieceId()
{
    RequestDefinition();
}

void AFEBuildPiece::OnRep_State()
{
    // PieceId 와 State 는 어느 순서로 도착할지 보장이 없다. ApplyState 는 정의가 아직 없으면 그냥 넘어간다.
    ApplyState();
}

void AFEBuildPiece::RequestDefinition()
{
    if (!PieceId.IsValid())
    {
        return;
    }

    UAssetManager& AssetManager = UAssetManager::Get();
    if (!AssetManager.GetPrimaryAssetPath(PieceId).IsValid())
    {
        UE_LOG(LogFEBuilding, Error, TEXT("%s: Asset Manager does not know %s. Check Primary Asset Types to Scan."), *GetName(), *PieceId.ToString());
        return;
    }

    // 이미 로드된 에셋이면 핸들이 null 로 오고 델리게이트는 즉시 실행된다. null 은 실패가 아니다.
    const TArray<FName> Bundles = { UFEBuildPieceDefinition::RuntimeBundle };
    DefinitionLoadHandle = AssetManager.LoadPrimaryAsset(
        PieceId, Bundles, FStreamableDelegate::CreateUObject(this, &AFEBuildPiece::HandleDefinitionLoaded));
}

void AFEBuildPiece::HandleDefinitionLoaded()
{
    Definition = Cast<UFEBuildPieceDefinition>(UAssetManager::Get().GetPrimaryAssetObject(PieceId));
    ApplyState();
}

void AFEBuildPiece::ApplyState()
{
    UStaticMesh* StaticMesh = Definition ? Definition->Mesh.Get() : nullptr;
    if (StaticMesh == nullptr)
    {
        // 정의가 아직 해석되지 않았거나(클라이언트) Runtime 번들이 로드되지 않음. 이후 OnRep/로드 콜백에서 다시 시도된다.
        return;
    }

    Mesh->SetStaticMesh(StaticMesh);
    Mesh->EmptyOverrideMaterials(); // Built 로 전환될 때 고스트/청사진 머티리얼을 걷어낸다

    switch (State)
    {
    case EFEBuildPieceState::Preview:
        Mesh->SetCollisionProfileName(TEXT("BuildPreview"));
        Mesh->SetCanEverAffectNavigation(false);
        SetPreviewValid(true);
        break;

    case EFEBuildPieceState::Blueprint:
        Mesh->SetCollisionProfileName(TEXT("BuildBlueprint"));
        Mesh->SetCanEverAffectNavigation(false);
        SetAllMaterials(Mesh, UFEBuildingSettings::Get()->BlueprintMaterial);
        break;

    case EFEBuildPieceState::Built:
        Mesh->SetCollisionProfileName(TEXT("BuildPiece"));
        Mesh->SetCanEverAffectNavigation(true);
        break;
    }

    OnStateChanged(State);
}
