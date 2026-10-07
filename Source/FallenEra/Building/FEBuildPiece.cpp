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
#include "Combat/FECombatTeams.h"

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
    DOREPLIFETIME(AFEBuildPiece, HealthPercent);
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
    
    // 청사진은 피해를 받지 않으므로 처음부터 최대 체력으로 두고, 완성되는 순간에도 그대로 쓴다
    Health = InDefinition ? InDefinition->MaxHealth : 0.f;

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
    if (!bCanChange) return;

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
    if (!bCanSupply) return false;

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
    if (Available <= 0) return false;

    const int32 Removed = Inventory.RemoveItems(ItemTag, Available);
    if (Removed <= 0) return false;
    
    SuppliedCounts[Index] += Removed;
    OnRep_SuppliedCounts(); // 서버 로컬 반영

    TryComplete();
    return true;
}

bool AFEBuildPiece::TryComplete()
{
    const bool bCanTry = HasAuthority() && State == EFEBuildPieceState::Blueprint && IsFullySupplied();
    if (!bCanTry) return false;

    if (!UFEBuildingSubsystem::CanComplete(this))
    {
        UE_LOG(LogFEBuilding, Log, TEXT("%s: fully supplied, waiting for support"), *GetName());
        return false;
    }
    SetState(EFEBuildPieceState::Built);
    return true;
}

bool AFEBuildPiece::CanDemolish(FText& OutReason) const
{
    return true;
}

void AFEBuildPiece::Demolish(IFEBuildInventoryProvider* Inventory)
{
    if (!HasAuthority()) return;

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
    if (!HasAuthority()) return;

    // 지금은 그냥 사라진다. 
    // 아이템 월드 스폰 API 가 오면 (1) 붕괴 연출(토대 제거 → 위 구조물 순차 낙하) 뒤 (2) 체력과 무관하게 루팅 아이템으로 변환한다
    UE_LOG(LogFEBuilding, Log, TEXT("%s collapsed (support %d)"), *GetName(), SupportDistance);
    Destroy();
}

void AFEBuildPiece::ApplyStructureDamage(float Amount, const AActor* Source)
{
    const bool bCanDamage = HasAuthority() && Definition != nullptr && State == EFEBuildPieceState::Built
        && Amount > 0.f && !IsActorBeingDestroyed() && UFEBuildingSettings::Get()->bStructureDamageEnabled;
    if (!bCanDamage) return;

    SetHealth(Health - Amount);
    UE_LOG(LogFEBuilding, Verbose, TEXT("%s took %.0f from %s (%.0f / %.0f)"),
        *GetName(), Amount, *GetNameSafe(Source), Health, Definition->MaxHealth);

    if (Health <= 0.f)
    {
        // 위 구조물은 EndPlay → UnregisterPiece → 지지 재계산에서 연쇄 붕괴한다 (철거·붕괴와 같은 경로)
        // ponytail: 보관함·화로 내용물은 사라진다. 아이템 월드 스폰 API 가 오면 바닥에 떨군다
        UE_LOG(LogFEBuilding, Log, TEXT("%s destroyed by %s"), *GetName(), *GetNameSafe(Source));
        Destroy();
    }
}

bool AFEBuildPiece::TryRepair(IFEBuildInventoryProvider& Inventory, FText& OutReason)
{
    const bool bCanRepair = HasAuthority() && Definition != nullptr && State == EFEBuildPieceState::Built && !IsActorBeingDestroyed();
    if (!bCanRepair) return false;

    const float MaxHealth = Definition->MaxHealth;
    const float Missing = MaxHealth - Health;
    if (Missing <= 0.f)
    {
        OutReason = LOCTEXT("RepairNotDamaged", "손상되지 않았습니다");
        return false;
    }

    // 1) 가득 수리 비용 = 깎인 비율 × 재료 개수 × RepairCostRate (항목마다 올림)
    const TArray<FFEBuildItemCost>& Required = Definition->RequiredItems;
    const float CostScale = Missing / MaxHealth * UFEBuildingSettings::Get()->RepairCostRate;
    TArray<int32> FullCosts;
    FullCosts.SetNum(Required.Num());

    // 2) 가장 부족한 재료 기준으로 수리 비율(0~1)을 정한다. 이 비율로 깎으면 어떤 항목도 가진 수를 넘지 않는다
    float Fraction = 1.f;
    for (int32 Index = 0; Index < Required.Num(); ++Index)
    {
        FullCosts[Index] = FMath::CeilToInt(Required[Index].Count * CostScale);
        if (FullCosts[Index] > 0)
        {
            const int32 Owned = Inventory.CountItems(Required[Index].ItemTag);
            Fraction = FMath::Min(Fraction, static_cast<float>(Owned) / FullCosts[Index]);
        }
    }
    if (Fraction <= 0.f)
    {
        OutReason = LOCTEXT("RepairNoItems", "수리할 재료가 부족합니다");
        return false;
    }

    // 3) 인벤토리에서 먼저 빼고 회복한다. ceil(비용 × 비율) ≤ 보유량이라 같은 서버 프레임 안에서는 부족해지지 않는다
    FString CostText;
    for (int32 Index = 0; Index < Required.Num(); ++Index)
    {
        const int32 Cost = FMath::CeilToInt(FullCosts[Index] * Fraction);
        if (Cost > 0)
        {
            const int32 Removed = Inventory.RemoveItems(Required[Index].ItemTag, Cost);
            CostText += FString::Printf(TEXT("%s %d "), *Required[Index].ItemTag.GetTagName().ToString(), Removed);
        }
    }

    const float OldHealth = Health;
    // 가득 수리는 부동소수 오차로 99% 에 머물지 않게 최대값을 직접 넣는다
    SetHealth(Fraction >= 1.f ? MaxHealth : Health + Missing * Fraction);
    UE_LOG(LogFEBuilding, Log, TEXT("%s repaired %.0f -> %.0f / %.0f (cost: %s)"), *GetName(), OldHealth, Health, MaxHealth, *CostText);
    return true;
}

FFE_CombatDamageResult AFEBuildPiece::ReceiveCombatDamage_Implementation(const FFE_CombatDamageRequest& DamageRequest)
{
    FFE_CombatDamageResult Result;

    // 청사진은 근접 판정(Enemy 채널 Overlap)에 잡히지만 피해 대상이 아니다 → 처리 안 함으로 돌려 적의 타격 수를 소모하지 않게 한다.
    // 같은 팀 검사는 HT 의 ApplyDamageInternal 도 하지만, 다른 호출 경로(함정·폭발물 등)를 대비해 여기서도 한다.
    const bool bCanReceive = HasAuthority() && State == EFEBuildPieceState::Built
        && !FECombatTeams::AreSameTeam(DamageRequest.SourceActor.Get(), this);
    if (!bCanReceive) return Result;

    ApplyStructureDamage(DamageRequest.SourceAttackPower, DamageRequest.SourceActor.Get());

    // 맞은 것으로 처리 → 적 근접 공격의 최대 타격 수를 벽이 소모해 벽 뒤 플레이어는 맞지 않는다.
    // bImpactCueHandled 는 false 로 두어 HT 쪽이 타격 이펙트를 재생한다.
    Result.bHandled = true;
    Result.bDamageApplied = true;
    return Result;
}

FGenericTeamId AFEBuildPiece::GetGenericTeamId() const
{
    // 플레이어 팀 → 플레이어의 공격·수류탄은 HT 의 같은 팀 검사에서 걸러진다.
    // 플레이어도 건물을 부술 수 있게 바꾸려면 여기서 FGenericTeamId::NoTeam 을 반환한다
    return FECombatTeams::Player;
}

void AFEBuildPiece::SetSupportDistances(uint8 InDesignDistance, uint8 InBuiltDistance)
{
    if (!HasAuthority()) return;

    DesignSupportDistance = InDesignDistance;
    SupportDistance = InBuiltDistance;
}

void AFEBuildPiece::WriteRecord(FFEBuildPieceRecord& OutRecord) const
{
    OutRecord.PieceId = PieceId;
    OutRecord.Location = GetActorLocation();
    OutRecord.Yaw = GetActorRotation().Yaw;
    OutRecord.State = State;
    OutRecord.SuppliedCounts = SuppliedCounts;
    OutRecord.Health = IsDamaged() ? Health : 0.f; // 0 = 가득
}

void AFEBuildPiece::ReadRecord(const FFEBuildPieceRecord& Record)
{
    // State 와 PieceId 는 앞서 호출된 InitializePiece 가 이미 잡았다. 여기서는 그게 덮어쓴 투입량만 되돌린다.
    // 정의가 바뀌어 재료 항목 수가 달라졌을 수 있으므로 현재 정의 길이에 맞춘다 (인덱스 기반 접근이 깨지지 않게).
    SuppliedCounts = Record.SuppliedCounts;
    SuppliedCounts.SetNum(Definition ? Definition->RequiredItems.Num() : 0);
    if (Record.Health > 0.f)
    {
        SetHealth(Record.Health);
    }
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
    if (!HasAuthority()) return; // [Server Only]

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

float AFEBuildPiece::GetHealthPercent() const
{
    return HealthPercent / 100.f;
}

bool AFEBuildPiece::IsDamaged() const
{
    return State == EFEBuildPieceState::Built && HealthPercent < 100;
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

void AFEBuildPiece::OnRep_HealthPercent()
{
    OnHealthChanged(GetHealthPercent());
}

void AFEBuildPiece::SetHealth(float NewHealth)
{
    const float MaxHealth = Definition ? Definition->MaxHealth : 1.f;
    Health = FMath::Clamp(NewHealth, 0.f, MaxHealth);

    // 가득 = 100, 손상 = 1~99. 내림 + 최소 1 이라 조금만 깎여도 클라에서 "손상"으로 보인다
    const uint8 NewPercent = Health >= MaxHealth
        ? 100
        : static_cast<uint8>(FMath::Clamp(FMath::FloorToInt(Health / MaxHealth * 100.f), 1, 99));
    if (NewPercent == HealthPercent) return;

    HealthPercent = NewPercent;
    if (HasActorBegunPlay())
    {
        OnRep_HealthPercent(); // 서버 로컬 반영. 복원 중(FinishSpawning 전)에는 BP 이벤트를 부르지 않는다
    }
}

void AFEBuildPiece::RequestDefinition()
{
    if (!PieceId.IsValid()) return;

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
    if (MeshComponent == nullptr || Material == nullptr) return;

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
    if (StaticMesh == nullptr)return; // 정의가 아직 해석되지 않았거나 Runtime 번들이 로드되지 않음. 이후 콜백에서 다시 시도된다.

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
