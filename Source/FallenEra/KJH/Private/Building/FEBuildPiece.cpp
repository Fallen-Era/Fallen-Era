// Fill out your copyright notice in the Description page of Project Settings.

#include "Building/FEBuildPiece.h"
#include "Building/FEBuildInventoryProvider.h"
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
    DOREPLIFETIME(AFEBuildPiece, SuppliedCount);
}

void AFEBuildPiece::InitializePiece(const UFEBuildPieceDefinition* InDefinition, EFEBuildPieceState InState)
{
    Definition = InDefinition;
    PieceId = InDefinition ? InDefinition->GetPrimaryAssetId() : FPrimaryAssetId();
    State = InState;
    
    // 재료가 필요 없는 피스(또는 InstantBuild)는 바로 완성. 진행도도 1 이 되도록 맞춘다.
    const bool bNothingToSupply = GetTotalRequired() == 0;
    if (State == EFEBuildPieceState::Built || bNothingToSupply)
    {
        State = EFEBuildPieceState::Built;
        SuppliedCount = GetTotalRequired();
    }

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

bool AFEBuildPiece::TrySupply(IFEBuildInventoryProvider& Inventory)
{
    const bool bCanSupply = HasAuthority() && Definition != nullptr && State == EFEBuildPieceState::Blueprint;
    if (!bCanSupply)
    {
        return false;
    }

    int32 SuppliedNow = 0;
    FGameplayTag ItemTag;
    int32 Remaining = 0;
    while (GetNextRequiredItem(ItemTag, Remaining))
    {
        const int32 Available = FMath::Min(Remaining, Inventory.CountItems(ItemTag));
        if (Available <= 0)
        {
            break; // 순서상 다음 재료가 없으면 여기서 멈춘다 (뒤 재료를 먼저 넣지 않음)
        }
        const int32 Removed = Inventory.RemoveItems(ItemTag, Available);
        if (Removed <= 0)
        {
            break;
        }
        SuppliedCount += Removed;
        SuppliedNow += Removed;
    }

    if (SuppliedNow > 0)
    {
        OnRep_SuppliedCount(); // 서버 로컬에서도 BP 이벤트를 받도록
    }

    const bool bIsComplete = SuppliedCount >= GetTotalRequired();
    if (bIsComplete)
    {
        SetState(EFEBuildPieceState::Built);
    }
    return SuppliedNow > 0 || bIsComplete;
}

void AFEBuildPiece::Demolish(IFEBuildInventoryProvider* Inventory)
{
    if (!HasAuthority())
    {
        return;
    }

    if (Inventory && Definition)
    {
        // 청사진은 투입한 만큼 전부, 완성품은 RefundRate 만큼 돌려준다. 투입 순서(RequiredItems)대로 되짚어 환불.
        const float Rate = State == EFEBuildPieceState::Built ? Definition->RefundRate : 1.f;
        int32 Cursor = SuppliedCount;
        for (const FFEBuildItemCost& Cost : Definition->RequiredItems)
        {
            const int32 SuppliedOfThis = FMath::Clamp(Cursor, 0, Cost.Count);
            const int32 Refund = FMath::FloorToInt(SuppliedOfThis * Rate);
            if (Refund > 0)
            {
                Inventory->AddItems(Cost.ItemTag, Refund);
            }
            Cursor -= SuppliedOfThis;
            if (Cursor <= 0)
            {
                break;
            }
        }
    }

    Destroy();
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

int32 AFEBuildPiece::GetTotalRequired() const
{
    int32 Total = 0;
    if (Definition)
    {
        for (const FFEBuildItemCost& Cost : Definition->RequiredItems)
        {
            Total += Cost.Count;
        }
    }
    return Total;
}

float AFEBuildPiece::GetSupplyProgress() const
{
    const int32 Total = GetTotalRequired();
    if (Total <= 0)
    {
        return 1.f;
    }
    return FMath::Clamp(static_cast<float>(SuppliedCount) / Total, 0.f, 1.f);
}

bool AFEBuildPiece::GetNextRequiredItem(FGameplayTag& OutItemTag, int32& OutRemaining) const
{
    if (Definition == nullptr)
    {
        return false;
    }

    int32 Cursor = SuppliedCount;
    for (const FFEBuildItemCost& Cost : Definition->RequiredItems)
    {
        if (Cursor < Cost.Count)
        {
            OutItemTag = Cost.ItemTag;
            OutRemaining = Cost.Count - Cursor;
            return true;
        }
        Cursor -= Cost.Count;
    }
    return false;
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

void AFEBuildPiece::OnRep_SuppliedCount()
{
    OnSupplyChanged(SuppliedCount, GetTotalRequired());
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
    OnSupplyChanged(SuppliedCount, GetTotalRequired()); // 정의가 늦게 와도 진행도 표시가 맞도록
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
