// 파일: Source/FallenEra/Building/FEBuildHudComponent.cpp
// 위치: 새 파일 (전체)
// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildHudComponent.h"
#include "FEBuildBallista.h"
#include "FEBuildFurnace.h"
#include "FEBuildingComponent.h"
#include "FEBuildingGameplayTags.h"
#include "FEBuildingSettings.h"
#include "FEBuildingSubsystem.h"
#include "FEBuildingViewModel.h"
#include "FEBuildInventoryProvider.h"
#include "FEBuildPiece.h"
#include "FEBuildPieceDefinition.h"
#include "FEBuildStorage.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

namespace
{
    /** "나무 3, 돌 2" — 수리·업그레이드 HUD 문구 공용 */
    FText FormatItemCosts(const TArray<FFEBuildItemCost>& Costs)
    {
        TArray<FText> Parts;
        for (const FFEBuildItemCost& Cost : Costs)
        {
            Parts.Add(FText::Format(LOCTEXT("ItemCostEntry", "{0} {1}"),
                UFEBuildingSettings::Get()->GetItemDisplayName(Cost.ItemTag), FText::AsNumber(Cost.Count)));
        }
        return FText::Join(FText::FromString(TEXT(", ")), Parts);
    }

    /** 구조물은 건설 탭, 방어시설(함정·발리스타)은 방어 탭, 그 외(가구·이동)는 가구 탭 */
    bool MatchesMenuTab(const UFEBuildPieceDefinition& Definition, EFEBuildMenuTab Tab)
    {
        const EFEBuildMenuTab PieceTab = Definition.Category == FEBuildingTags::Build_Piece_Structure ? EFEBuildMenuTab::Construction
            : Definition.Category == FEBuildingTags::Build_Piece_Defense ? EFEBuildMenuTab::Defense
            : EFEBuildMenuTab::Furniture;
        return Tab == PieceTab;
    }

    FText GetMenuTabTitle(EFEBuildMenuTab Tab)
    {
        switch (Tab)
        {
        case EFEBuildMenuTab::Construction: return LOCTEXT("TabConstruction", "건설");
        case EFEBuildMenuTab::Defense:      return LOCTEXT("TabDefense", "방어");
        default:                            return LOCTEXT("TabFurniture", "가구");
        }
    }

    /** 투입 패널 선택지: 재질 미정이면 사슬 전체(나무·돌·철), 정해졌으면 현재 재질 하나.
     *  RebuildSupplyRows 와 RefreshSupplyRows 가 같은 순서를 써야 줄과 숫자가 맞는다 */
    void GetSupplyVariants(const AFEBuildPiece& Piece, TArray<const UFEBuildPieceDefinition*>& OutVariants)
    {
        OutVariants.Reset();
        const UFEBuildPieceDefinition* Definition = Piece.GetDefinition();
        if (Definition == nullptr) return;

        if (Piece.IsMaterialLocked())
        {
            OutVariants.Add(Definition);
        }
        else
        {
            Definition->GetMaterialChain(OutVariants);
        }
    }
}

UFEBuildHudComponent::UFEBuildHudComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true); // 패널 조작 Server RPC 를 쓰려면 필수
}

void UFEBuildHudComponent::BeginPlay()
{
    Super::BeginPlay();

    // 빌드 모드·선택 피스는 건설 컴포넌트가 가진다. 바뀔 때 알림을 받아 HUD 를 맞춘다 (같은 액터의 컴포넌트는 BeginPlay 전에 모두 만들어져 있다)
    BuildingComponent = GetOwner()->FindComponentByClass<UFEBuildingComponent>();
    if (UFEBuildingComponent* Building = BuildingComponent.Get())
    {
        Building->OnBuildStateChangedNative.AddUObject(this, &UFEBuildHudComponent::HandleBuildStateChanged);
    }
    else
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("%s has no UFEBuildingComponent; build menu and aimed-piece HUD are disabled"), *GetNameSafe(GetOwner()));
    }

    if (GetNetMode() != NM_DedicatedServer)
    {
        // 조준 HUD(망치 체력 줄·발리스타 탄약). 데디케이티드 서버는 화면이 없다. 원격 폰은 UpdateAimedPiece 가 걸러낸다
        // (상호작용 프롬프트와 같은 10Hz 폴링: "무엇을 조준하는지"는 카메라가 돌 때마다 바뀌는데 이벤트가 없다)
        GetWorld()->GetTimerManager().SetTimer(AimTimer, this, &UFEBuildHudComponent::UpdateAimedPiece, 0.1f, true);

        // HUD 를 모르는 쪽(건설 컴포넌트·발리스타)이 보내는 안내 문구. 이 기계의 플레이어 폰만 띄운다 (다른 플레이어 폰의 뷰모델은 화면에 없다)
        if (UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(GetWorld()))
        {
            Subsystem->OnLocalNoticeNative.AddWeakLambda(this, [this](const FText& Text)
            {
                const APawn* Pawn = Cast<APawn>(GetOwner());
                if (Pawn != nullptr && Pawn->IsLocallyControlled())
                {
                    ShowNotice(Text);
                }
            });
        }
    }
}

void UFEBuildHudComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UFEBuildingComponent* Building = BuildingComponent.Get())
    {
        Building->OnBuildStateChangedNative.RemoveAll(this);
    }
    if (UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(GetWorld()))
    {
        Subsystem->OnLocalNoticeNative.RemoveAll(this);
    }
    CloseSupplyPanel();
    CloseStoragePanel();
    GetWorld()->GetTimerManager().ClearTimer(AimTimer);
    Super::EndPlay(EndPlayReason);
}

void UFEBuildHudComponent::HandleBuildStateChanged()
{
    const UFEBuildingComponent* Building = BuildingComponent.Get();
    if (Building == nullptr) return;

    const bool bInBuildMode = Building->IsInBuildMode();
    GetViewModel()->SetBuildMode(bInBuildMode);

    // 피스를 고른 직후(번들 로드 전)에는 이전 이름을 그대로 둔다 — 로드가 끝나면 알림이 한 번 더 온다
    if (const UFEBuildPieceDefinition* Selected = Building->GetSelectedPiece())
    {
        GetViewModel()->SetSelectedPieceName(Selected->DisplayName);
    }
    const FPrimaryAssetId SelectedId = Building->GetSelectedPieceId();
    for (UObject* Entry : GetViewModel()->PieceEntries)
    {
        if (UFEBuildPieceEntryViewModel* EntryViewModel = Cast<UFEBuildPieceEntryViewModel>(Entry))
        {
            EntryViewModel->SetSelected(EntryViewModel->PieceId == SelectedId);
        }
    }

    if (!bInBuildMode)
    {
        CloseBuildMenu(); // 빌드 모드를 나가면(취소·B) 메뉴도 닫는다
    }
}

void UFEBuildHudComponent::ToggleBuildMenu()
{
    // Tab 이 탄약 UI 를 맡는 경우: 열려 있으면 닫고, 탑승 중이거나(빌드 모드 밖에서) 완성 발리스타를 조준하고 있으면 연다
    if (bIsAmmoPanel)
    {
        CloseStoragePanel();
        return;
    }
    if (AFEBuildBallista* AmmoTarget = FindAmmoPanelTarget())
    {
        OpenStoragePanel(AmmoTarget);
        return;
    }
    if (bIsMenuOpen)
    {
        CloseBuildMenu();
        return;
    }

    UFEBuildingComponent* Building = BuildingComponent.Get();
    if (Building == nullptr) return; // 피스를 고를 건설 컴포넌트가 없으면 메뉴도 없다
    if (!Building->IsInBuildMode())
    {
        Building->ToggleBuildMode();
    }

    bIsMenuOpen = true;
    GetViewModel()->SetMenuOpen(true);
    UpdateUIMode();

    // 목록에 이름을 띄우려면 정의 에셋 자체가 필요하다. 번들 없이 DA 만 로드 (메시는 안 따라온다).
    TArray<FPrimaryAssetId> PieceIds;
    UAssetManager::Get().GetPrimaryAssetIdList(UFEBuildPieceDefinition::AssetType, PieceIds);
    MenuLoadHandle = UAssetManager::Get().LoadPrimaryAssets(
        PieceIds, TArray<FName>(), FStreamableDelegate::CreateUObject(this, &UFEBuildHudComponent::BuildPieceEntries));
}

void UFEBuildHudComponent::CloseBuildMenu()
{
    if (!bIsMenuOpen) return;
    bIsMenuOpen = false;

    GetViewModel()->SetMenuOpen(false);
    UpdateUIMode();
}

void UFEBuildHudComponent::SelectMenuTab(EFEBuildMenuTab Tab)
{
    MenuTab = Tab;
    // 정의 에셋은 메뉴를 열 때 MenuLoadHandle 로 이미 로드돼 있다 → 다시 로드하지 않고 목록만 다시 채운다
    BuildPieceEntries();
}

void UFEBuildHudComponent::SelectPieceFromMenu(FPrimaryAssetId PieceId)
{
    if (UFEBuildingComponent* Building = BuildingComponent.Get())
    {
        Building->SelectPiece(PieceId);
    }
    CloseBuildMenu();
}

void UFEBuildHudComponent::BuildPieceEntries()
{
    if (!bIsMenuOpen) return; // 로드 중에 닫힘

    TArray<FPrimaryAssetId> PieceIds;
    UAssetManager::Get().GetPrimaryAssetIdList(UFEBuildPieceDefinition::AssetType, PieceIds);

    // 메뉴 순서값 → 에셋 이름순. 메뉴를 열 때 정의는 이미 전부 로드돼 있다
    UAssetManager& AssetManager = UAssetManager::Get();
    PieceIds.Sort([&AssetManager](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
    {
        const UFEBuildPieceDefinition* DefinitionA = Cast<UFEBuildPieceDefinition>(AssetManager.GetPrimaryAssetObject(A));
        const UFEBuildPieceDefinition* DefinitionB = Cast<UFEBuildPieceDefinition>(AssetManager.GetPrimaryAssetObject(B));
        const int32 OrderA = DefinitionA ? DefinitionA->MenuOrder : 0;
        const int32 OrderB = DefinitionB ? DefinitionB->MenuOrder : 0;
        return OrderA != OrderB ? OrderA < OrderB : A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName);
    });

    // 다른 정의의 UpgradeTo 대상(돌·철 변형)은 메뉴에 내지 않는다 — 재질은 재료를 넣을 때 고른다.
    // 소프트 경로 비교라 추가 로드가 없다 (메뉴를 열 때 정의는 이미 전부 로드됨)
    TSet<FSoftObjectPath> VariantPaths;
    for (const FPrimaryAssetId& PieceId : PieceIds)
    {
        const UFEBuildPieceDefinition* Definition = Cast<UFEBuildPieceDefinition>(UAssetManager::Get().GetPrimaryAssetObject(PieceId));
        if (Definition && !Definition->UpgradeTo.IsNull())
        {
            VariantPaths.Add(Definition->UpgradeTo.ToSoftObjectPath());
        }
    }

    const UFEBuildingComponent* Building = BuildingComponent.Get();
    const FPrimaryAssetId SelectedId = Building ? Building->GetSelectedPieceId() : FPrimaryAssetId();

    TArray<UObject*> Entries;
    for (const FPrimaryAssetId& PieceId : PieceIds)
    {
        const UFEBuildPieceDefinition* Definition = Cast<UFEBuildPieceDefinition>(UAssetManager::Get().GetPrimaryAssetObject(PieceId));
        const bool bShow = Definition != nullptr && MatchesMenuTab(*Definition, MenuTab) && !VariantPaths.Contains(FSoftObjectPath(Definition));
        if (!bShow) continue;

        // 재질과 무관한 이름("벽")이 있으면 그것으로 — 재질은 투입할 때 고르므로
        const FText Name = !Definition->BaseName.IsEmpty() ? Definition->BaseName
            : !Definition->DisplayName.IsEmpty() ? Definition->DisplayName
            : FText::FromName(PieceId.PrimaryAssetName);

        UFEBuildPieceEntryViewModel* Entry = NewObject<UFEBuildPieceEntryViewModel>(GetViewModel());
        Entry->Initialize(this, PieceId, Name);
        Entry->SetSelected(PieceId == SelectedId);
        Entries.Add(Entry);
    }
    GetViewModel()->SetPieceEntries(Entries);
    GetViewModel()->SetMenuTabTitle(GetMenuTabTitle(MenuTab));
}

void UFEBuildHudComponent::OpenSupplyPanel(AFEBuildPiece* Piece)
{
    if (Piece == nullptr || Piece == SupplyTarget)
    {
        CloseSupplyPanel();
        return;
    }
    CloseSupplyPanel();

    SupplyTarget = Piece;
    SupplyTarget->OnSupplyChangedNative.AddUObject(this, &UFEBuildHudComponent::RefreshSupplyRows);
    SupplyTarget->OnDestroyed.AddDynamic(this, &UFEBuildHudComponent::HandleSupplyTargetDestroyed);

    RebuildSupplyRows();
    GetViewModel()->SetSupplyPanelOpen(true);
    RefreshSupplyRows();
    UpdateUIMode();

    // [넣기] 활성은 소지품을 알아야 한다. 소지품은 리플리케이트되지 않으므로 스냅샷을 받는다 (보관함 패널과 같은 경로)
    ServerRequestInventory();
}

void UFEBuildHudComponent::CloseSupplyPanel()
{
    if (SupplyTarget)
    {
        SupplyTarget->OnSupplyChangedNative.RemoveAll(this);
        SupplyTarget->OnDestroyed.RemoveDynamic(this, &UFEBuildHudComponent::HandleSupplyTargetDestroyed);
        SupplyTarget = nullptr;
    }
    SupplyRowsDefinition = nullptr;
    bSupplyRowsLocked = false;

    GetViewModel()->SetSupplyPanelOpen(false);
    GetViewModel()->SetSupplyRows(TArray<UObject*>());
    UpdateUIMode();
}

void UFEBuildHudComponent::SupplyItem(FGameplayTag ItemTag)
{
    if (SupplyTarget && ItemTag.IsValid())
    {
        ServerSupplyItem(SupplyTarget, ItemTag);
    }
}

void UFEBuildHudComponent::RefreshSupplyRows()
{
    const UFEBuildPieceDefinition* Definition = SupplyTarget ? SupplyTarget->GetDefinition() : nullptr;
    if (Definition == nullptr) return;

    if (SupplyTarget->IsFullySupplied())
    {
        CloseSupplyPanel(); // 다 찼다. 완성 여부는 지지 구조가 결정하므로 패널은 여기서 끝
        return;
    }

    // 재질이 정해졌으면(나 또는 다른 플레이어가 먼저) 선택지 → 한 재질로 줄 구성이 바뀐다. 그 외에는 숫자만 바꿔 깜빡이지 않게
    if (Definition != SupplyRowsDefinition.Get() || SupplyTarget->IsMaterialLocked() != bSupplyRowsLocked)
    {
        RebuildSupplyRows();
    }

    TArray<const UFEBuildPieceDefinition*> Variants;
    GetSupplyVariants(*SupplyTarget, Variants);

    const TArray<TObjectPtr<UObject>>& Rows = GetViewModel()->SupplyRows;
    int32 RowIndex = 0;
    for (const UFEBuildPieceDefinition* Variant : Variants)
    {
        for (int32 ItemIndex = 0; ItemIndex < Variant->RequiredItems.Num(); ++ItemIndex, ++RowIndex)
        {
            UFESupplyRowViewModel* Row = Rows.IsValidIndex(RowIndex) ? Cast<UFESupplyRowViewModel>(Rows[RowIndex]) : nullptr;
            if (Row == nullptr) continue;

            // 투입량은 현재 재질의 것만 의미가 있다 (재질 미정 선택지는 전부 0)
            const FFEBuildItemCost& Cost = Variant->RequiredItems[ItemIndex];
            const int32 Supplied = Variant == Definition ? SupplyTarget->GetSuppliedCount(ItemIndex) : 0;
            Row->SetCounts(Supplied, Cost.Count, GetCarriedCount(Cost.ItemTag));
        }
    }
}

void UFEBuildHudComponent::RebuildSupplyRows()
{
    TArray<const UFEBuildPieceDefinition*> Variants;
    if (SupplyTarget)
    {
        GetSupplyVariants(*SupplyTarget, Variants);
    }

    const UFEBuildPieceDefinition* Definition = SupplyTarget ? SupplyTarget->GetDefinition() : nullptr;
    SupplyRowsDefinition = Definition;
    bSupplyRowsLocked = SupplyTarget && SupplyTarget->IsMaterialLocked();

    // 선택지가 둘 이상이면 줄마다 변형 이름("돌 벽 — 돌 0 / 6")을 붙이고, 두 번째 선택지부터 첫 줄 위에 "또는"
    const bool bIsChoice = Variants.Num() > 1;
    TArray<UObject*> Rows;
    for (int32 VariantIndex = 0; VariantIndex < Variants.Num(); ++VariantIndex)
    {
        const UFEBuildPieceDefinition* Variant = Variants[VariantIndex];
        for (int32 ItemIndex = 0; ItemIndex < Variant->RequiredItems.Num(); ++ItemIndex)
        {
            const FGameplayTag ItemTag = Variant->RequiredItems[ItemIndex].ItemTag;
            UFESupplyRowViewModel* Row = NewObject<UFESupplyRowViewModel>(GetViewModel());
            Row->Initialize(this, ItemTag, UFEBuildingSettings::Get()->GetItemDisplayName(ItemTag),
                bIsChoice ? Variant->DisplayName : FText::GetEmpty(),
                bIsChoice && VariantIndex > 0 && ItemIndex == 0);
            Rows.Add(Row);
        }
    }
    GetViewModel()->SetSupplyRows(Rows);

    if (Definition == nullptr) return;

    if (bIsChoice)
    {
        const FText BaseName = Definition->BaseName.IsEmpty() ? Definition->DisplayName : Definition->BaseName;
        GetViewModel()->SetSupplyTitle(FText::Format(LOCTEXT("SupplyChooseTitle", "{0} — 재질 선택"), BaseName));
        GetViewModel()->SetSupplyHint(LOCTEXT("SupplyChooseHint", "재료를 하나 골라 넣으세요. 처음 넣은 재료로 재질이 정해집니다"));
    }
    else
    {
        GetViewModel()->SetSupplyTitle(Definition->DisplayName);
        // 구조물은 다른 재질이 있다 → 바꾸는 방법을 알려 준다. 가구는 재질이 하나라 안내 없음
        const bool bHasAlternatives = Definition->Category == FEBuildingTags::Build_Piece_Structure;
        GetViewModel()->SetSupplyHint(bHasAlternatives
            ? LOCTEXT("SupplyLockedHint", "다른 재료로 바꾸려면 철거하세요 (청사진은 100% 환불)")
            : FText::GetEmpty());
    }
}

int32 UFEBuildHudComponent::GetCarriedCount(FGameplayTag ItemTag) const
{
    int32 Total = 0;
    for (const FFEBuildItemCost& Item : CarriedItems)
    {
        if (Item.ItemTag == ItemTag)
        {
            Total += Item.Count;
        }
    }
    return Total;
}

void UFEBuildHudComponent::HandleSupplyTargetDestroyed(AActor* DestroyedActor)
{
    CloseSupplyPanel();
}

void UFEBuildHudComponent::OpenStoragePanel(AFEBuildStorage* Storage)
{
    if (Storage == nullptr || Storage == StorageTarget)
    {
        CloseStoragePanel();
        return;
    }
    CloseStoragePanel();

    StorageTarget = Storage;
    StorageTarget->OnContentsChangedNative.AddUObject(this, &UFEBuildHudComponent::RefreshStorageSlots);
    StorageTarget->OnDestroyed.AddDynamic(this, &UFEBuildHudComponent::HandleStorageTargetDestroyed);

    // 보관함 칸은 열 때 Capacity 개를 한 번 만들고 이후엔 내용만 바꾼다. TileView 가 매번 새로 그려지며 깜빡이지 않게.
    TArray<UObject*> Slots;
    for (int32 Index = 0; Index < Storage->GetCapacity(); ++Index)
    {
        UFEItemSlotViewModel* Slot = NewObject<UFEItemSlotViewModel>(GetViewModel());
        Slot->Initialize(this, true, Index);
        Slots.Add(Slot);
    }
    GetViewModel()->SetStorageSlots(Slots);

    const UFEBuildPieceDefinition* Definition = Storage->GetDefinition();
    GetViewModel()->SetStorageTitle(Definition ? Definition->DisplayName : FText::GetEmpty());
    // 발리스타는 전용 탄약 위젯, 그 외는 보관함 패널. 칸 목록(StorageSlots·CarriedSlots)은 같은 것을 쓴다
    bIsAmmoPanel = Storage->IsA<AFEBuildBallista>();
    if (bIsAmmoPanel)
    {
        GetViewModel()->SetAmmoPanelOpen(true);
    }
    else
    {
        GetViewModel()->SetStoragePanelOpen(true);
    }

    // 화로면 진행 바 줄을 보이고 점화·진행도 변화를 받는다
    AFEBuildFurnace* Furnace = Cast<AFEBuildFurnace>(Storage);
    if (Furnace)
    {
        Furnace->OnFurnaceStateChangedNative.AddUObject(this, &UFEBuildHudComponent::RefreshFurnaceState);
    }
    GetViewModel()->SetFurnaceVisible(Furnace != nullptr);

    RefreshFurnaceState();
    RefreshStorageSlots();      // 보관함 내용만으로 먼저 그린다
    ServerRequestInventory();   // 소지품이 도착하면 ClientReceiveInventory 가 다시 그린다
    UpdateUIMode();
}

void UFEBuildHudComponent::CloseStoragePanel()
{
    if (StorageTarget)
    {
        StorageTarget->OnContentsChangedNative.RemoveAll(this);
        StorageTarget->OnDestroyed.RemoveDynamic(this, &UFEBuildHudComponent::HandleStorageTargetDestroyed);
        if (AFEBuildFurnace* Furnace = Cast<AFEBuildFurnace>(StorageTarget))
        {
            Furnace->OnFurnaceStateChangedNative.RemoveAll(this);
        }
        StorageTarget = nullptr;
    }
    CarriedItems.Reset();
    GetViewModel()->SetStoragePanelOpen(false);
    GetViewModel()->SetAmmoPanelOpen(false);
    bIsAmmoPanel = false;
    GetViewModel()->SetStorageSlots(TArray<UObject*>());
    GetViewModel()->SetCarriedSlots(TArray<UObject*>());
    GetViewModel()->SetFurnaceVisible(false);
    UpdateUIMode();
}

void UFEBuildHudComponent::StoreStack(FGameplayTag ItemTag, int32 Count)
{
    if (StorageTarget == nullptr || !ItemTag.IsValid() || Count <= 0) return;

    // 화로는 연료·재료만 받는다. 클라가 먼저 판단해 바로 알려 주고, 서버 StoreItems 가 같은 검사를 다시 한다
    FText Reason;
    if (!StorageTarget->AcceptsItem(ItemTag, Reason))
    {
        ShowNotice(Reason);
        return;
    }
    ServerStoreItem(StorageTarget, ItemTag, Count);
}

void UFEBuildHudComponent::TakeSlot(int32 SlotIndex, FGameplayTag ItemTag)
{
    if (StorageTarget && ItemTag.IsValid() && SlotIndex >= 0)
    {
        ServerTakeItem(StorageTarget, SlotIndex, ItemTag);
    }
}

void UFEBuildHudComponent::RefreshStorageSlots()
{
    if (StorageTarget == nullptr) return;

    // 보관함: 앞쪽부터 찬 칸, 나머지는 빈 칸
    const TArray<FFEBuildItemCost>& Contents = StorageTarget->GetContents();
    const TArray<TObjectPtr<UObject>>& Slots = GetViewModel()->StorageSlots;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        UFEItemSlotViewModel* Slot = Cast<UFEItemSlotViewModel>(Slots[Index]);
        const bool bIsFilled = Contents.IsValidIndex(Index) && Contents[Index].Count > 0; // 화로는 빈 칸도 항목으로 있다
        if (bIsFilled)
        {
            Slot->SetItem(Contents[Index].ItemTag, Contents[Index].Count);
        }
        else
        {
            Slot->SetEmpty(StorageTarget->GetSlotLabel(Index));
        }
    }

    // 소지품: 보관함과 같은 스택 상한으로 나눠 보여 준다. 칸을 누르면 그 칸의 개수만큼 넣는다.
    // ponytail: 표시용 분할. 실제 인벤토리(병일)의 칸 구조가 오면 그쪽 칸을 그대로 쓴다.
    const int32 MaxStack = StorageTarget->GetMaxStackSize();
    TArray<UObject*> Carried;
    for (const FFEBuildItemCost& Entry : CarriedItems)
    {
        FText Ignored;
        if (bIsAmmoPanel && !StorageTarget->AcceptsItem(Entry.ItemTag, Ignored)) continue; // 탄약 위젯은 볼트만 보인다

        for (int32 Left = Entry.Count; Left > 0; Left -= MaxStack)
        {
            UFEItemSlotViewModel* Slot = NewObject<UFEItemSlotViewModel>(GetViewModel());
            Slot->Initialize(this, false, INDEX_NONE);
            Slot->SetItem(Entry.ItemTag, FMath::Min(Left, MaxStack));
            Carried.Add(Slot);
        }
    }

    GetViewModel()->SetCarriedSlots(Carried);
}

void UFEBuildHudComponent::HandleStorageTargetDestroyed(AActor* DestroyedActor)
{
    CloseStoragePanel();
}

void UFEBuildHudComponent::RefreshFurnaceState()
{
    if (const AFEBuildFurnace* Furnace = Cast<AFEBuildFurnace>(StorageTarget))
    {
        GetViewModel()->SetFurnaceState(Furnace->IsLit(), Furnace->GetProcessPercent() / 100.f);
    }
}

void UFEBuildHudComponent::ToggleFurnaceLit()
{
    AFEBuildFurnace* Furnace = Cast<AFEBuildFurnace>(StorageTarget);
    if (Furnace == nullptr) return;

    const bool bWantLit = !Furnace->IsLit();
    FText Reason;
    if (bWantLit && !Furnace->CanIgnite(Reason))
    {
        ShowNotice(Reason);
        return;
    }
    ServerSetFurnaceLit(Furnace, bWantLit);
}

void UFEBuildHudComponent::ShowNotice(const FText& Text)
{
    GetViewModel()->SetNotice(Text);

    // 2초 뒤 지운다. 연달아 오면 타이머가 처음부터 다시 돈다
    GetWorld()->GetTimerManager().SetTimer(NoticeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        GetViewModel()->SetNotice(FText::GetEmpty());
    }), 2.f, false);
}

UFEBuildingViewModel* UFEBuildHudComponent::GetViewModel()
{
    if (ViewModel == nullptr)
    {
        ViewModel = NewObject<UFEBuildingViewModel>(this);
        ViewModel->Component = this;
    }
    return ViewModel;
}

void UFEBuildHudComponent::UpdateUIMode()
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PlayerController = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
    if (PlayerController == nullptr || !PlayerController->IsLocalController()) return;

    const bool bUIOpen = bIsMenuOpen || SupplyTarget != nullptr || StorageTarget != nullptr;
    PlayerController->SetShowMouseCursor(bUIOpen);
    if (bUIOpen)
    {
        // UI 전용: 메뉴가 열린 동안 시점 회전·빌드 입력을 막는다. 닫기는 버튼으로.
        FInputModeUIOnly Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PlayerController->SetInputMode(Mode);
    }
    else
    {
        PlayerController->SetInputMode(FInputModeGameOnly());
    }
}

void UFEBuildHudComponent::UpdateAimedPiece()
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (Pawn == nullptr) return;

    // 서버에서 원격 플레이어가 빙의한 폰은 이 기계의 화면과 무관하다 → 타이머를 끈다.
    // 클라의 다른 플레이어 폰(SimulatedProxy)과 빙의 전 폰은 끄지 않는다 — 늦게 빙의되면 내 폰이 될 수 있다 (판정 한 번이라 비용은 무시 가능)
    if (Pawn->HasAuthority() && Pawn->GetController() != nullptr && !Pawn->IsLocallyControlled())
    {
        GetWorld()->GetTimerManager().ClearTimer(AimTimer);
        return;
    }
    if (!Pawn->IsLocallyControlled()) return;

    UpdateBallistaHud(); // 탑승 HUD 도 같은 0.1초 타이머로 갱신한다

    // 망치를 들지 않았으면 트레이스도 하지 않는다. 조준 트레이스·망치 판정은 수리와 같은 것(건설 컴포넌트)을 쓴다
    const UFEBuildingComponent* Building = BuildingComponent.Get();
    const AFEBuildPiece* Piece = Building != nullptr && Building->IsHoldingRepairTool() ? Building->FindPieceUnderCrosshair() : nullptr;
    const bool bShow = Piece != nullptr && Piece->GetState() == EFEBuildPieceState::Built && Piece->GetDefinition() != nullptr;
    if (!bShow)
    {
        GetViewModel()->SetAimedPiece(FText::GetEmpty(), 1.f, FText::GetEmpty(), FText::GetEmpty());
        return;
    }

    const float Percent = Piece->GetHealthPercent();
    FText RepairText = LOCTEXT("RepairNotNeeded", "손상 없음");
    if (Piece->IsDamaged())
    {
        // 클라는 정확한 HP 를 모른다 → 리플리케이트된 % 로 추정 (내림이라 실제보다 최대 1개 많게 보일 수 있다)
        TArray<FFEBuildItemCost> Costs;
        Piece->GetRepairCost(1.f - Percent, Costs);
        RepairText = Costs.IsEmpty()
            ? LOCTEXT("RepairFree", "수리: 재료 없음")
            : FText::Format(LOCTEXT("RepairCost", "수리: {0}"), FormatItemCosts(Costs));
    }

    FText UpgradeText;
    if (const UFEBuildPieceDefinition* Target = Piece->GetUpgradeTarget())
    {
        UpgradeText = FText::Format(LOCTEXT("UpgradeHint", "우클릭 업그레이드: {0} ({1})"),
            Target->DisplayName, FormatItemCosts(Target->RequiredItems));
    }

    GetViewModel()->SetAimedPiece(Piece->GetDefinition()->DisplayName, Percent, RepairText, UpgradeText);
}

void UFEBuildHudComponent::UpdateBallistaHud()
{
    // 탑승은 발리스타가 처리한다. 여기서는 내가 탄 발리스타를 찾아 탄약·장전만 보여 준다
    AFEBuildBallista* Ballista = AFEBuildBallista::FindOperatedBy(GetOwner());
    if (Ballista == nullptr)
    {
        // 방금 내렸으면(발리스타가 부서진 경우 포함) 한 번만 숨기고, 그 발리스타의 탄약 패널이 열려 있으면 닫는다
        if (LastHudAmmo >= 0)
        {
            if (bIsAmmoPanel && StorageTarget != nullptr && StorageTarget == HudBallista.Get())
            {
                CloseStoragePanel();
            }
            LastHudAmmo = -1;
            GetViewModel()->SetBallistaHud(false, FText::GetEmpty());
        }
        return;
    }
    if (Ballista != HudBallista.Get())
    {
        HudBallista = Ballista;
        LastHudAmmo = -1; // 새로 탔다 → 이번에 무조건 그린다
    }

    const int32 AmmoCount = Ballista->GetAmmoCount();
    const bool bReloading = Ballista->IsReloading();
    if (AmmoCount == LastHudAmmo && bReloading == bLastHudReloading) return; // 바뀐 게 없으면 뷰모델 알림을 보내지 않는다

    LastHudAmmo = AmmoCount;
    bLastHudReloading = bReloading;
    const FText AmmoText = FText::Format(LOCTEXT("BallistaHud", "{0} {1} / {2}"),
        Ballista->GetSlotLabel(0), FText::AsNumber(AmmoCount), FText::AsNumber(Ballista->GetMaxStackSize()));
    GetViewModel()->SetBallistaHud(true, bReloading
        ? FText::Format(LOCTEXT("BallistaHudReloading", "{0} · 장전 중"), AmmoText) : AmmoText);
}

AFEBuildBallista* UFEBuildHudComponent::FindAmmoPanelTarget() const
{
    if (AFEBuildBallista* Mounted = AFEBuildBallista::FindOperatedBy(GetOwner())) return Mounted;

    // 빌드 모드의 Tab 은 빌드 메뉴. 조준은 건설 컴포넌트의 트레이스(철거·수리와 같은 시점·채널)를 쓴다
    const UFEBuildingComponent* Building = BuildingComponent.Get();
    if (Building == nullptr || Building->IsInBuildMode()) return nullptr;

    AFEBuildBallista* Aimed = Cast<AFEBuildBallista>(Building->FindPieceUnderCrosshair());
    return Aimed != nullptr && Aimed->GetState() == EFEBuildPieceState::Built ? Aimed : nullptr;
}

void UFEBuildHudComponent::ClientShowNotice_Implementation(const FText& Text)
{
    ShowNotice(Text);
}

bool UFEBuildHudComponent::ServerSupplyItem_Validate(AFEBuildPiece* Piece, FGameplayTag ItemTag)
{
    return Piece != nullptr && ItemTag.IsValid();
}

void UFEBuildHudComponent::ServerSupplyItem_Implementation(AFEBuildPiece* Piece, FGameplayTag ItemTag)
{
    if (!UFEBuildingComponent::IsPieceInReach(GetOwner(), Piece)) return;

    IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner());
    if (Inventory == nullptr)
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("%s has no IFEBuildInventoryProvider; cannot supply"), *GetNameSafe(GetOwner()));
        return;
    }

    // 실패 사유는 서버만 아는 것(다른 플레이어가 먼저 재질을 정함, 그새 재료가 없어짐)만 돌려준다
    FText Reason;
    if (!Piece->TrySupply(*Inventory, ItemTag, Reason) && !Reason.IsEmpty())
    {
        UE_LOG(LogFEBuilding, Log, TEXT("Supply rejected: %s (%s)"), *Piece->GetName(), *Reason.ToString());
        ClientShowNotice(Reason);
    }

    // 넣은 뒤 남은 소지품으로 [넣기] 버튼 활성 상태를 갱신하도록 스냅샷을 다시 보낸다
    SendInventorySnapshot();
}

bool UFEBuildHudComponent::ServerStoreItem_Validate(AFEBuildStorage* Storage, FGameplayTag ItemTag, int32 Count)
{
    return Storage != nullptr && ItemTag.IsValid() && Count > 0;
}

void UFEBuildHudComponent::ServerStoreItem_Implementation(AFEBuildStorage* Storage, FGameplayTag ItemTag, int32 Count)
{
    IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner());
    if (!UFEBuildingComponent::IsPieceInReach(GetOwner(), Storage) || Inventory == nullptr) return;

    // 클라가 보낸 개수는 화면 기준이다. 실제 보유량으로 다시 자른 뒤,
    // 보관함에 먼저 넣어 보고(칸 제한) 실제로 들어간 만큼만 인벤토리에서 뺀다. 순서가 반대면 아이템이 증발한다.
    const int32 Requested = FMath::Min(Count, Inventory->CountItems(ItemTag));
    const int32 Stored = Storage->StoreItems(ItemTag, Requested);
    if (Stored > 0)
    {
        Inventory->RemoveItems(ItemTag, Stored);
    }
    SendInventorySnapshot();
}

bool UFEBuildHudComponent::ServerTakeItem_Validate(AFEBuildStorage* Storage, int32 SlotIndex, FGameplayTag ItemTag)
{
    return Storage != nullptr && SlotIndex >= 0 && ItemTag.IsValid();
}

void UFEBuildHudComponent::ServerTakeItem_Implementation(AFEBuildStorage* Storage, int32 SlotIndex, FGameplayTag ItemTag)
{
    IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner());
    if (!UFEBuildingComponent::IsPieceInReach(GetOwner(), Storage) || Inventory == nullptr) return;

    // 넣기와 같은 원칙: 받는 쪽(인벤토리)에 먼저 넣고, 실제로 들어간 만큼만 칸에서 뺀다.
    // 되돌리기가 필요 없어서 화로처럼 아무 아이템이나 받지 않는 컨테이너에서도 증발하지 않는다.
    const int32 Available = Storage->GetSlotCount(SlotIndex, ItemTag);
    if (Available > 0)
    {
        const int32 Added = Inventory->AddItems(ItemTag, Available);
        Storage->TakeFromSlot(SlotIndex, ItemTag, Added);
    }
    SendInventorySnapshot();
}

bool UFEBuildHudComponent::ServerSetFurnaceLit_Validate(AFEBuildFurnace* Furnace, bool bLit)
{
    return Furnace != nullptr;
}

void UFEBuildHudComponent::ServerSetFurnaceLit_Implementation(AFEBuildFurnace* Furnace, bool bLit)
{
    if (UFEBuildingComponent::IsPieceInReach(GetOwner(), Furnace))
    {
        Furnace->SetLit(bLit); // 켤 때 CanIgnite 를 서버 값으로 다시 검사한다
    }
}

bool UFEBuildHudComponent::ServerRequestInventory_Validate()
{
    return true;
}

void UFEBuildHudComponent::ServerRequestInventory_Implementation()
{
    SendInventorySnapshot();
}

void UFEBuildHudComponent::SendInventorySnapshot()
{
    TArray<FFEBuildItemCost> Snapshot;
    if (IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner()))
    {
        Inventory->GetItems(Snapshot);
    }
    ClientReceiveInventory(Snapshot);
}

void UFEBuildHudComponent::ClientReceiveInventory_Implementation(const TArray<FFEBuildItemCost>& InItems)
{
    CarriedItems = InItems;
    RefreshStorageSlots();
    RefreshSupplyRows(); // 투입 패널이 열려 있으면 [넣기] 활성 갱신 (없으면 바로 빠져나감)
}

#undef LOCTEXT_NAMESPACE
