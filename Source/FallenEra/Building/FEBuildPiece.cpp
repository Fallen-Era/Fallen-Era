// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildPiece.h"
#include "FEBuildInventoryProvider.h"
#include "FEBuildPieceDefinition.h"
#include "FEBuildingSettings.h"
#include "FEBuildingSubsystem.h"
#include "FEBuildingComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

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
    DOREPLIFETIME(AFEBuildPiece, SuppliedCounts);
    DOREPLIFETIME(AFEBuildPiece, DesignSupportDistance);
    DOREPLIFETIME(AFEBuildPiece, SupportDistance);
}

void AFEBuildPiece::BeginPlay()
{
    Super::BeginPlay();

    const bool bIsPlacedOnServer = HasAuthority() && State != EFEBuildPieceState::Preview;
    if (bIsPlacedOnServer)
    {
        if (UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(GetWorld()))
        {
            Subsystem->RegisterPiece(this);
        }
    }
}

void AFEBuildPiece::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        if (UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(GetWorld()))
        {
            Subsystem->UnregisterPiece(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void AFEBuildPiece::InitializePiece(const UFEBuildPieceDefinition* InDefinition, EFEBuildPieceState InState)
{
    Definition = InDefinition;
    PieceId = InDefinition ? InDefinition->GetPrimaryAssetId() : FPrimaryAssetId();
    State = InState;

    const int32 ItemCount = InDefinition ? InDefinition->RequiredItems.Num() : 0;
    SuppliedCounts.Init(0, ItemCount);

    // 재료가 필요 없는 피스(또는 InstantBuild)는 바로 완성. 진행도도 1 이 되도록 맞춘다.
    const bool bNothingToSupply = GetTotalRequired() == 0;
    if (State == EFEBuildPieceState::Built || bNothingToSupply)
    {
        State = EFEBuildPieceState::Built;
        for (int32 Index = 0; Index < ItemCount; ++Index)
        {
            SuppliedCounts[Index] = InDefinition->RequiredItems[Index].Count;
        }
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

    if (UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(GetWorld()))
    {
        Subsystem->MarkDirty();
    }
}

bool AFEBuildPiece::TrySupply(IFEBuildInventoryProvider& Inventory, FGameplayTag ItemTag)
{
    const bool bCanSupply = HasAuthority() && Definition != nullptr && State == EFEBuildPieceState::Blueprint;
    if (!bCanSupply)
    {
        return false;
    }

    const int32 Index = Definition->RequiredItems.IndexOfByPredicate([&ItemTag](const FFEBuildItemCost& Cost)
    {
        return Cost.ItemTag == ItemTag;
    });
    if (!SuppliedCounts.IsValidIndex(Index))
    {
        return false; // 이 피스가 요구하지 않는 재료
    }

    const int32 Remaining = Definition->RequiredItems[Index].Count - SuppliedCounts[Index];
    const int32 Available = FMath::Min(Remaining, Inventory.CountItems(ItemTag));
    if (Available <= 0)
    {
        return false;
    }

    const int32 Removed = Inventory.RemoveItems(ItemTag, Available);
    if (Removed <= 0)
    {
        return false;
    }
    SuppliedCounts[Index] += Removed;
    OnRep_SuppliedCounts(); // 서버 로컬 반영

    TryComplete();
    return true;
}

bool AFEBuildPiece::TryComplete()
{
    const bool bCanTry = HasAuthority() && State == EFEBuildPieceState::Blueprint && IsFullySupplied();
    if (!bCanTry)
    {
        return false;
    }
    if (!UFEBuildingSubsystem::CanComplete(this))
    {
        UE_LOG(LogFEBuilding, Log, TEXT("%s: fully supplied, waiting for support"), *GetName());
        return false;
    }
    SetState(EFEBuildPieceState::Built);
    return true;
}

void AFEBuildPiece::Demolish(IFEBuildInventoryProvider* Inventory)
{
    if (!HasAuthority())
    {
        return;
    }

    if (Inventory && Definition)
    {
        // 청사진은 투입한 만큼 전부, 완성품은 RefundRate 만큼. 투입 순서대로 되짚어 환불.
        const float Rate = State == EFEBuildPieceState::Built ? Definition->RefundRate : 1.f;
        for (int32 Index = 0; Index < Definition->RequiredItems.Num(); ++Index)
        {
            const int32 Supplied = SuppliedCounts.IsValidIndex(Index) ? SuppliedCounts[Index] : 0;
            const int32 Refund = FMath::FloorToInt(Supplied * Rate);
            if (Refund > 0)
            {
                Inventory->AddItems(Definition->RequiredItems[Index].ItemTag, Refund);
            }
        }
    }

    Destroy();
}

void AFEBuildPiece::Collapse()
{
    if (!HasAuthority())
    {
        return;
    }
    // 지금은 그냥 사라진다. 
    // 아이템 월드 스폰 API 가 오면 (1) 붕괴 연출(토대 제거 → 위 구조물 순차 낙하) 뒤 (2) 체력과 무관하게 루팅 아이템으로 변환한다
    UE_LOG(LogFEBuilding, Log, TEXT("%s collapsed (support %d)"), *GetName(), SupportDistance);
    Destroy();
}

void AFEBuildPiece::SetSupportDistances(uint8 InDesignDistance, uint8 InBuiltDistance)
{
    if (!HasAuthority())
    {
        return;
    }
    DesignSupportDistance = InDesignDistance;
    SupportDistance = InBuiltDistance;
}

void AFEBuildPiece::SetPreviewValid(bool bIsValid)
{
    const UFEBuildingSettings* Settings = UFEBuildingSettings::Get();
    ApplyMaterialToAll(Mesh, bIsValid ? Settings->GhostValidMaterial : Settings->GhostInvalidMaterial);
}

// ---- IFEInteractable ----

bool AFEBuildPiece::CanInteract_Implementation(AActor* InstigatorActor) const
{
    switch (State)
    {
    case EFEBuildPieceState::Blueprint:
        return Definition != nullptr; // 재료 투입
    case EFEBuildPieceState::Built:
        return CanInteractBuilt(InstigatorActor);
    default:
        return false;
    }
}

FText AFEBuildPiece::GetInteractText_Implementation(AActor* InstigatorActor) const
{
    if (State == EFEBuildPieceState::Built)
    {
        return GetInteractTextBuilt(InstigatorActor);
    }

    if (IsFullySupplied())
    {
        return LOCTEXT("WaitingSupport", "지지 구조 완성 대기");
    }
    return FText::Format(LOCTEXT("SupplyPrompt", "재료 투입 ({0} / {1})"), FText::AsNumber(GetTotalSupplied()), FText::AsNumber(GetTotalRequired()));
}

void AFEBuildPiece::Interact_Implementation(AActor* InstigatorActor)
{
    if (!HasAuthority())
    {
        return; // [Server Only]
    }

    if (State == EFEBuildPieceState::Blueprint)
    {
        return; // 투입은 클라 패널(InteractLocal)에서 항목별 ServerSupplyItem 으로 요청한다
    }

    if (State == EFEBuildPieceState::Built)
    {
        InteractBuilt(InstigatorActor);
    }
}

void AFEBuildPiece::InteractLocal_Implementation(AActor* InstigatorActor)
{
    switch (State)
    {
    case EFEBuildPieceState::Blueprint:
        // 요청한 클라이언트에서 투입 패널을 연다. 패널 상태는 그 플레이어의 건설 컴포넌트가 가진다.
        if (UFEBuildingComponent* Building = InstigatorActor ? InstigatorActor->FindComponentByClass<UFEBuildingComponent>() : nullptr)
        {
            Building->OpenSupplyPanel(this);
        }
        break;
    case EFEBuildPieceState::Built:
        InteractBuiltLocal(InstigatorActor);
        break;
    default:
        break;
    }
}

bool AFEBuildPiece::CanInteractBuilt(AActor* InstigatorActor) const
{
    return false;
}

FText AFEBuildPiece::GetInteractTextBuilt(AActor* InstigatorActor) const
{
    return FText::GetEmpty();
}

void AFEBuildPiece::InteractBuilt(AActor* InstigatorActor)
{
}

void AFEBuildPiece::InteractBuiltLocal(AActor* InstigatorActor)
{
}

void AFEBuildPiece::OnApplyState(EFEBuildPieceState NewState)
{
}

// ---- 조회 ----

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

bool AFEBuildPiece::IsAnchor() const
{
    return Definition != nullptr && Definition->bCanPlaceOnGround;
}

int32 AFEBuildPiece::GetSupportCost() const
{
    return Definition ? Definition->SupportCost : 0;
}

uint8 AFEBuildPiece::GetDesignSupportDistance() const
{
    return DesignSupportDistance;
}

uint8 AFEBuildPiece::GetSupportDistance() const
{
    return SupportDistance;
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

bool AFEBuildPiece::IsFullySupplied() const
{
    return Definition != nullptr && GetTotalSupplied() >= GetTotalRequired();
}

float AFEBuildPiece::GetSupplyProgress() const
{
    const int32 Total = GetTotalRequired();
    if (Total <= 0)
    {
        return 1.f;
    }
    return FMath::Clamp(static_cast<float>(GetTotalSupplied()) / Total, 0.f, 1.f);
}

int32 AFEBuildPiece::GetSuppliedCount(int32 Index) const
{
    return SuppliedCounts.IsValidIndex(Index) ? SuppliedCounts[Index] : 0;
}

int32 AFEBuildPiece::GetTotalSupplied() const
{
    int32 Total = 0;
    for (const int32 Count : SuppliedCounts)
    {
        Total += Count;
    }
    return Total;
}

// ---- 리플리케이션 / 로드 / 시각 ----

void AFEBuildPiece::OnRep_PieceId()
{
    RequestDefinition();
}

void AFEBuildPiece::OnRep_State()
{
    ApplyState();
}

void AFEBuildPiece::OnRep_SuppliedCounts()
{
    OnSupplyChanged(GetTotalSupplied(), GetTotalRequired());
    OnSupplyChangedNative.Broadcast();
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
    OnSupplyChanged(GetTotalSupplied(), GetTotalRequired());
    OnSupplyChangedNative.Broadcast();
}

void AFEBuildPiece::ApplyMaterialToAll(UStaticMeshComponent* MeshComponent, const TSoftObjectPtr<UMaterialInterface>& SoftMaterial)
{
    // 고스트/청사진 머티리얼은 작은 Unlit 머티리얼이라 첫 사용 시 동기 로드해도 부담이 없다.
    UMaterialInterface* Material = SoftMaterial.LoadSynchronous();
    if (MeshComponent == nullptr || Material == nullptr)
    {
        return;
    }
    for (int32 SlotIndex = 0; SlotIndex < MeshComponent->GetNumMaterials(); ++SlotIndex)
    {
        MeshComponent->SetMaterial(SlotIndex, Material);
    }
}

FName AFEBuildPiece::GetCollisionProfileForState(EFEBuildPieceState PieceState)
{
    switch (PieceState)
    {
    case EFEBuildPieceState::Preview:   return TEXT("BuildPreview");
    case EFEBuildPieceState::Blueprint: return TEXT("BuildBlueprint");
    default:                            return TEXT("BuildPiece");
    }
}

void AFEBuildPiece::ApplyState()
{
    UStaticMesh* StaticMesh = Definition ? Definition->Mesh.Get() : nullptr;
    if (StaticMesh == nullptr)
    {
        return; // 정의가 아직 해석되지 않았거나 Runtime 번들이 로드되지 않음. 이후 콜백에서 다시 시도된다.
    }

    Mesh->SetStaticMesh(StaticMesh);
    Mesh->EmptyOverrideMaterials();
    Mesh->SetCollisionProfileName(GetCollisionProfileForState(State));
    Mesh->SetCanEverAffectNavigation(State == EFEBuildPieceState::Built);

    switch (State)
    {
    case EFEBuildPieceState::Preview:
        SetPreviewValid(true);
        break;
    case EFEBuildPieceState::Blueprint:
        ApplyMaterialToAll(Mesh, UFEBuildingSettings::Get()->BlueprintMaterial);
        break;
    default:
        break;
    }

    OnApplyState(State);
    OnStateChanged(State);
}

#undef LOCTEXT_NAMESPACE
