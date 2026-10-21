// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildingComponent.h"
#include "FEBuildInventoryProvider.h"
#include "FEBuildPiece.h"
#include "FEBuildPieceDefinition.h"
#include "FEBuildingSettings.h"
#include "FEBuildingSubsystem.h"
#include "FEBuildingViewModel.h"
#include "FEBuildStorage.h"
#include "FEBuildFurnace.h"
#include "FEBuildingGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DrawDebugHelpers.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Stats/Stats.h"
#include "Engine/OverlapResult.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Combat/FECombatGameplayTags.h"
#include "FallenEraCharacter.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "InputMappingContext.h"

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
    
    /** 구조물은 건설 탭, 그 외(가구·방어시설·이동)는 가구 탭 */
    bool MatchesMenuTab(const UFEBuildPieceDefinition& Definition, EFEBuildMenuTab Tab)
    {
        const bool bIsStructure = Definition.Category == FEBuildingTags::Build_Piece_Structure;
        return Tab == (bIsStructure ? EFEBuildMenuTab::Construction : EFEBuildMenuTab::Furniture);
    }

    FText GetMenuTabTitle(EFEBuildMenuTab Tab)
    {
        return Tab == EFEBuildMenuTab::Construction ? LOCTEXT("TabConstruction", "건설") : LOCTEXT("TabFurniture", "가구");
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

// 테스트용: 1 이면 배치 즉시 완성(재료 투입 생략). 기본은 청사진 경로.
static TAutoConsoleVariable<bool> CVarFEInstantBuild(
    TEXT("fe.Build.InstantBuild"), false,
    TEXT("Place pieces as Built instead of Blueprint (skips material fill)."));

// 디버그: 프리뷰 주변 피스의 소켓(파랑=상대 전용, 노랑=붙는 쪽)과 프리뷰 소켓(초록)을 그린다.
static TAutoConsoleVariable<bool> CVarFEDebugSockets(
    TEXT("fe.Build.DebugSockets"), false,
    TEXT("Draw build sockets around the preview."));

// 디버그: 빌드 모드 중 주변 피스 위에 지지 거리를 표시한다. D = 설계(청사진 포함), B = 완성 그래프.
static TAutoConsoleVariable<bool> CVarFEShowSupport(
    TEXT("fe.Build.ShowSupport"), false,
    TEXT("Show support distances above nearby pieces while in build mode."));

namespace
{
    /** 클라·서버 사이 위치 차이(네트워크 지연)를 흡수하는 거리 여유. 거리 검사 두 곳이 반드시 같은 값을 써야 한다 */
    constexpr float ReachSlack = 300.f;

    /** 배치 트랜스폼이 그대로여도 이 간격마다 다시 검증한다 (다른 플레이어가 바로 옆에 지은 경우) */
    constexpr double PreviewRevalidateInterval = 0.25;

    /** [Server] 한 플레이어의 배치 요청 최소 간격. 사람 손으로는 넘을 수 없는 속도 */
    constexpr double MinPlaceInterval = 0.1;
    
    /** 배치 오버랩 상자를 바운드보다 줄이는 양. 옆면 = 코너 직각 벽·이웃 토대 소켓 어긋남 허용(같은 자리 중복은 중심이 겹쳐 여전히 잡힌다),
     *  위아래 = 지면 접촉·천장 아래 끼우기 */
    constexpr float SideClearance = 22.f;
    constexpr float FaceClearance = 2.f;
    
    /** 소켓 없는 피스(가구·함정)의 옆 여유. 맞닿기만 허용하고 겹침은 막는다 */
    constexpr float FreeSideClearance = 1.f;

    /** 지지 면적(MinSupportCoverage) 검사: 바닥을 N×N 칸으로 나눠 각 칸 중앙 아래에서 표면을 찾는다 */
    constexpr int32 CoverageSamples = 4;

    /** 칸 배치 기준 판으로 인정하는 윗면 (법선 Z). 지붕 같은 경사면은 기준이 되지 않는다 */
    constexpr float GridBoardMinNormalZ = 0.95f;

    /** bRequiresFoundationBelow: 피스 중심 아래 이 깊이까지 anchor(토대)를 찾는다. 2층 천장은 1층 천장을 지나 토대를 찾는다 */
    constexpr float FoundationSearchDepth = 2000.f;

    /** [Client] 고스트가 움직이는 동안의 최소 검증 간격. 클릭 순간에는 ConfirmPlacement 가 따로 검증한다 */
    constexpr double PreviewMovingValidateInterval = 0.05;

    /** [Server] 요청 간격 제한. 받을 때마다 NextAllowedTime 을 Interval 씩 누적한다 —
     *  지터로 일찍 온 요청(Interval 의 절반까지)은 받되, 아무리 빨리 보내도 평균 간격은 Interval */
    bool ConsumeRequestSlot(double& NextAllowedTime, double Now, double Interval)
    {
        if (Now < NextAllowedTime - Interval * 0.5) return false;
        NextAllowedTime = FMath::Max(NextAllowedTime, Now) + Interval;
        return true;
    }
    
    UClass* ResolvePieceClass(const UFEBuildPieceDefinition* Piece)
    {
        UClass* LoadedClass = Piece->PieceClass.Get();
        return LoadedClass ? LoadedClass : AFEBuildPiece::StaticClass();
    }

    uint8 YawToStep(float YawDegrees)
    {
        const UFEBuildingSettings* Settings = UFEBuildingSettings::Get();
        const int32 StepCount = Settings->GetYawStepCount();
        const float NormalizedYaw = FMath::Fmod(YawDegrees + 360.f, 360.f);
        const int32 Step = FMath::RoundToInt(NormalizedYaw / Settings->RotationStepDeg) % StepCount;
        return static_cast<uint8>(Step);
    }

    /**
     * 아래로 트레이스해 놓일 표면을 찾는다. 지형(WorldStatic/WorldDynamic)이 기본이고,
     * bIncludeStructures 면 구조물(소켓 있는 BuildPiece — 토대·천장)도 표면으로 인정한다 (가구·함정을 그 위에 놓을 때).
     * 가구·함정(소켓 없는 피스)은 표면이 아니다 — 그 위로 올라타 쌓이거나, 옆 함정 모서리에 코너가 걸려 들뜨지 않게
     */
    bool TraceSurface(const UWorld* World, const FVector& Point, float Slack, const AActor* IgnoreActor, bool bIncludeStructures, FHitResult& OutHit)
    {
        FCollisionObjectQueryParams SurfaceTypes;
        SurfaceTypes.AddObjectTypesToQuery(ECC_WorldStatic);
        SurfaceTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
        if (bIncludeStructures)
        {
            SurfaceTypes.AddObjectTypesToQuery(ECC_FEBuildPiece);
        }

        // 오브젝트 타입 멀티 트레이스는 맞은 것을 전부(가까운 순) 돌려준다 → 위에서부터 첫 "표면"을 고른다
        const FVector Offset(0.f, 0.f, Slack);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(FEBuildGround), false, IgnoreActor);
        TArray<FHitResult> Hits;
        World->LineTraceMultiByObjectType(Hits, Point + Offset, Point - Offset, SurfaceTypes, Params);
        for (const FHitResult& Hit : Hits)
        {
            const AFEBuildPiece* Piece = Cast<AFEBuildPiece>(Hit.GetActor());
            const bool bIsAttachment = Piece != nullptr && Piece->GetDefinition() != nullptr && Piece->GetDefinition()->Sockets.IsEmpty();
            if (!bIsAttachment)
            {
                OutHit = Hit;
                return true;
            }
        }
        return false;
    }

    /** 바닥 4코너의 로컬 위치 (Z = 메시 하단) */
    void GetBottomCorners(const FBox& LocalBounds, FVector OutCorners[4])
    {
        OutCorners[0] = FVector(LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Min.Z);
        OutCorners[1] = FVector(LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Min.Z);
        OutCorners[2] = FVector(LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Min.Z);
        OutCorners[3] = FVector(LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Min.Z);
    }
    
    /** 바닥면을 CoverageSamples×CoverageSamples 칸으로 나눈 각 칸 중앙 아래 Slack 안에 표면이 있는 비율 (0~1) */
    float GetSupportCoverage(const UWorld* World, const FBox& LocalBounds, const FTransform& Transform, float Slack, const AActor* IgnoreActor, bool bIncludeStructures)
    {
        int32 Hits = 0;
        for (int32 IndexX = 0; IndexX < CoverageSamples; ++IndexX)
        {
            for (int32 IndexY = 0; IndexY < CoverageSamples; ++IndexY)
            {
                const FVector LocalPoint(
                    FMath::Lerp(LocalBounds.Min.X, LocalBounds.Max.X, (IndexX + 0.5f) / CoverageSamples),
                    FMath::Lerp(LocalBounds.Min.Y, LocalBounds.Max.Y, (IndexY + 0.5f) / CoverageSamples),
                    LocalBounds.Min.Z);
                FHitResult SurfaceHit;
                if (TraceSurface(World, Transform.TransformPosition(LocalPoint), Slack, IgnoreActor, bIncludeStructures, SurfaceHit))
                {
                    ++Hits;
                }
            }
        }
        return static_cast<float>(Hits) / (CoverageSamples * CoverageSamples);
    }

    /**
     * 칸 배치(GridCellSize > 0) 피스의 위치·회전을 칸에 맞춘다. 칸 크기는 DA 값이고 메시 크기와 무관하다.
     * 1) 커서가 평평한 구조물(소켓 있는 피스 — 토대·천장) 윗면 → 그 판을 칸으로 나눈 칸 중앙 (판 밖으로 나가지 않음)
     * 2) 아니면 SnapRadius 안 가장 가까운 칸 배치 피스 → 거기서 칸 단위로 이어 붙임 (함정끼리 스냅)
     * 3) 둘 다 없으면 그대로 (자유 배치)
     * 높이는 이후 표면 맞춤이 정한다. 클라 편의 기능이라 서버는 결과 위치만 검증한다(겹침·지지 면적)
     */
    void SnapToGrid(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FHitResult* CursorHit, int32 YawStepOffset, const AActor* IgnoreActor, FVector& InOutLocation, uint8& InOutYawStep)
    {
        const double Cell = Piece->GridCellSize;
        if (Cell < 1.0) return;

        const AFEBuildPiece* Board = CursorHit ? Cast<AFEBuildPiece>(CursorHit->GetActor()) : nullptr;
        const bool bBoardIsStructure = Board != nullptr && Board->GetDefinition() != nullptr && !Board->GetDefinition()->Sockets.IsEmpty();
        const UStaticMesh* BoardMesh = bBoardIsStructure ? Board->GetDefinition()->Mesh.Get() : nullptr;
        const bool bOnFlatBoard = BoardMesh != nullptr && CursorHit->ImpactNormal.Z >= GridBoardMinNormalZ;

        const AFEBuildPiece* Anchor = bOnFlatBoard ? Board : nullptr;
        if (Anchor == nullptr)
        {
            TArray<AFEBuildPiece*> Nearby;
            UFEBuildingSubsystem::GatherNearbyPieces(World, InOutLocation, UFEBuildingSettings::Get()->SnapRadius, IgnoreActor, Nearby);
            float BestDistSq = TNumericLimits<float>::Max();
            for (const AFEBuildPiece* Other : Nearby)
            {
                const float DistSq = FVector::DistSquared2D(Other->GetActorLocation(), InOutLocation);
                if (Other->GetDefinition()->GridCellSize > 0.f && DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    Anchor = Other;
                }
            }
        }
        if (Anchor == nullptr) return;

        const FTransform AnchorXf = Anchor->GetActorTransform();
        FVector Local = AnchorXf.InverseTransformPosition(InOutLocation);
        if (bOnFlatBoard)
        {
            // 판 바운드를 칸으로 나눈 칸 중앙. 커서가 판 가장자리 밖이어도 가장자리 칸에 멈춘다
            const FBox BoardBounds = BoardMesh->GetBoundingBox();
            const auto SnapToCell = [Cell](double Value, double Min, double Max)
            {
                const int32 Count = FMath::Max(1, FMath::FloorToInt((Max - Min) / Cell + 0.01));
                const int32 Index = FMath::Clamp(FMath::FloorToInt((Value - Min) / Cell), 0, Count - 1);
                return Min + (Index + 0.5) * Cell;
            };
            Local.X = SnapToCell(Local.X, BoardBounds.Min.X, BoardBounds.Max.X);
            Local.Y = SnapToCell(Local.Y, BoardBounds.Min.Y, BoardBounds.Max.Y);
        }
        else
        {
            // 기준 피스에서 칸 단위로 이어 붙인다
            Local.X = FMath::RoundToDouble(Local.X / Cell) * Cell;
            Local.Y = FMath::RoundToDouble(Local.Y / Cell) * Cell;
        }

        // 커서가 함정(소켓 없는 피스) 윗면이면 그 높이 대신 기준 피스의 바닥 높이에서 시작한다 —
        // 안 그러면 고스트가 함정 위에 떠서 "Not enough support" 로 보인다. 최종 높이는 아래 표면 맞춤이 정한다
        const bool bCursorOnAttachment = Board != nullptr && Board->GetDefinition() != nullptr && !bBoardIsStructure;
        const double KeepZ = bCursorOnAttachment ? AnchorXf.GetLocation().Z : InOutLocation.Z;
        InOutLocation = AnchorXf.TransformPosition(Local);
        InOutLocation.Z = KeepZ;

        // 회전 = 기준 회전 + Q/E 90° 단위. 기준이 15° 단계로 놓였으므로 YawStep 으로 그대로 표현된다
        InOutYawStep = YawToStep(AnchorXf.Rotator().Yaw + (YawStepOffset % 4) * 90.f);
    }

    const FTransform& GetFlip180()
    {
        static const FTransform Flip(FRotator(0.f, 180.f, 0.f));
        return Flip;
    }

    /** 소켓 하나를 그린다. 구체 = 위치, 선 = X축(바깥 방향) */
    void DrawSocket(const UWorld* World, const FFEBuildSocket& Socket, const FTransform& OwnerXf, const FColor& Color)
    {
        const FTransform SocketWorld = Socket.LocalTransform * OwnerXf;
        const FVector Location = SocketWorld.GetLocation();
        DrawDebugSphere(World, Location, 8.f, 8, Color, false, -1.f, 0, 1.f);
        DrawDebugLine(World, Location, Location + SocketWorld.GetUnitAxis(EAxis::X) * 40.f, Color, false, -1.f, 0, 2.f);
    }
}

UFEBuildingComponent::UFEBuildingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; // 빌드 모드 중에만 켠다
    SetIsReplicatedByDefault(true); // 컴포넌트에서 Server RPC 를 쓰려면 필수
}

void UFEBuildingComponent::ToggleBuildMode()
{
    if (bIsInBuildMode)
    {
        CancelBuild();
        return;
    }
    bIsInBuildMode = true;
    UpdateBuildModeState();
    UE_LOG(LogFEBuilding, Log, TEXT("Build mode on"));
    YawStepOffset = 0;
    SetComponentTickEnabled(true);
    GetViewModel()->SetBuildMode(true);
    SelectPiece(DefaultPieceId);
}

void UFEBuildingComponent::SelectPiece(FPrimaryAssetId InPieceId)
{
    DestroyPreview();
    SelectedPiece = nullptr;
    SelectedPieceId = InPieceId;

    if (!InPieceId.IsValid()) return;

    UAssetManager& AssetManager = UAssetManager::Get();
    if (!AssetManager.GetPrimaryAssetPath(InPieceId).IsValid())
    {
        UE_LOG(LogFEBuilding, Error, TEXT("Asset Manager does not know %s. Check Primary Asset Types to Scan."), *InPieceId.ToString());
        return;
    }

    // 이미 로드된 에셋이면 핸들이 null 로 오고 델리게이트는 즉시 실행된다. null 은 실패가 아니다.
    const TArray<FName> Bundles = { UFEBuildPieceDefinition::RuntimeBundle };
    PreviewLoadHandle = AssetManager.LoadPrimaryAsset(
        InPieceId, Bundles, FStreamableDelegate::CreateUObject(this, &UFEBuildingComponent::HandlePreviewAssetsLoaded, InPieceId));
}

void UFEBuildingComponent::HandlePreviewAssetsLoaded(FPrimaryAssetId LoadedPieceId)
{
    const bool bStillWanted = bIsInBuildMode && LoadedPieceId == SelectedPieceId;
    if (!bStillWanted) return; // 로드 중에 다른 피스를 고르거나 빌드 모드를 나간 경우
    
    SelectedPiece = Cast<UFEBuildPieceDefinition>(UAssetManager::Get().GetPrimaryAssetObject(LoadedPieceId));
    UE_LOG(LogFEBuilding, Log, TEXT("Selected piece: %s"), *LoadedPieceId.ToString());
    
    GetViewModel()->SetSelectedPieceName(SelectedPiece ? SelectedPiece->DisplayName : FText::GetEmpty());
    for (UObject* Entry : GetViewModel()->PieceEntries)
    {
        if (UFEBuildPieceEntryViewModel* EntryViewModel = Cast<UFEBuildPieceEntryViewModel>(Entry))
        {
            EntryViewModel->SetSelected(EntryViewModel->PieceId == LoadedPieceId);
        }
    }
}

void UFEBuildingComponent::RotatePreview(int32 Direction)
{
    const int32 StepCount = UFEBuildingSettings::Get()->GetYawStepCount();
    YawStepOffset = (YawStepOffset + FMath::Sign(Direction) + StepCount) % StepCount;
}

void UFEBuildingComponent::ConfirmPlacement()
{
    // 고스트가 보이는(배치 위치를 구한) 상태여야 한다. 고스트 색은 이동 중 최대 PreviewMovingValidateInterval 늦으므로 클릭한 위치로 다시 검증한다
    const bool bHasPlacement = bIsInBuildMode && SelectedPiece != nullptr && PreviewActor != nullptr && !PreviewActor->IsHidden();
    if (!bHasPlacement) return;
    if (!ValidatePlacement(GetWorld(), SelectedPiece, MakePlacementTransform(PreviewLocation, PreviewYawStep), GetOwner())) return;

    ServerPlacePiece(SelectedPieceId, PreviewLocation, PreviewYawStep);
}

void UFEBuildingComponent::CancelBuild()
{
    const bool bWasInBuildMode = bIsInBuildMode;
    bIsInBuildMode = false;
    UpdateBuildModeState();
    if (bWasInBuildMode)
    {
        UE_LOG(LogFEBuilding, Log, TEXT("Build mode off")); // 취소 입력은 빌드 모드 밖에서도 오므로 실제로 나갈 때만
    }
    SetComponentTickEnabled(false);
    DestroyPreview();
    SelectedPiece = nullptr;
    PreviewLoadHandle.Reset();
    GetViewModel()->SetBuildMode(false);
    CloseBuildMenu();
}

void UFEBuildingComponent::DemolishPiece()
{
    AFEBuildPiece* Piece = FindPieceUnderCrosshair();
    if (Piece == nullptr) return;

    // 내용물은 리플리케이트되므로 클라가 먼저 판단해 바로 알려 준다. 서버도 같은 검사를 다시 한다.
    FText Reason;
    if (!Piece->CanDemolish(Reason))
    {
        UE_LOG(LogFEBuilding, Log, TEXT("Demolish blocked: %s (%s)"), *Piece->GetName(), *Reason.ToString());
        ShowNotice(Reason);
        return;
    }
    ServerDemolishPiece(Piece);
}

void UFEBuildingComponent::RepairPiece()
{
    if (!TrySwingTool(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick)) return;

    // 손상 여부는 리플리케이트된 % 로 클라가 먼저 판단한다. 대상이 없어도 서버에는 알린다 (다른 플레이어에게 휘두르기 표시)
    AFEBuildPiece* Piece = FindPieceUnderCrosshair();
    const bool bIsBuilt = Piece != nullptr && Piece->GetState() == EFEBuildPieceState::Built;
    if (bIsBuilt && !Piece->IsDamaged())
    {
        ShowNotice(LOCTEXT("RepairNotDamagedClient", "손상되지 않았습니다"));
    }
    ServerRepairPiece(bIsBuilt && Piece->IsDamaged() ? Piece : nullptr);
}

void UFEBuildingComponent::UpgradePiece()
{
    if (!TrySwingTool(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick)) return;

    // 다음 단계가 있는지는 클라도 안다 (대상 DA 가 Runtime 번들로 로드돼 있음). 재료는 서버만 안다
    AFEBuildPiece* Piece = FindPieceUnderCrosshair();
    const bool bIsBuilt = Piece != nullptr && Piece->GetState() == EFEBuildPieceState::Built;
    const bool bHasTarget = bIsBuilt && Piece->GetUpgradeTarget() != nullptr;
    if (bIsBuilt && !bHasTarget)
    {
        ShowNotice(LOCTEXT("UpgradeMaxClient", "더 이상 업그레이드할 수 없습니다"));
    }
    ServerUpgradePiece(bHasTarget ? Piece : nullptr);
}

void UFEBuildingComponent::ToggleBuildMenu()
{
    if (bIsMenuOpen)
    {
        CloseBuildMenu();
        return;
    }
    if (!bIsInBuildMode)
    {
        ToggleBuildMode();
    }

    bIsMenuOpen = true;
    GetViewModel()->SetMenuOpen(true);
    UpdateUIMode();

    // 목록에 이름을 띄우려면 정의 에셋 자체가 필요하다. 번들 없이 DA 만 로드 (메시는 안 따라온다).
    TArray<FPrimaryAssetId> PieceIds;
    UAssetManager::Get().GetPrimaryAssetIdList(UFEBuildPieceDefinition::AssetType, PieceIds);
    MenuLoadHandle = UAssetManager::Get().LoadPrimaryAssets(
        PieceIds, TArray<FName>(), FStreamableDelegate::CreateUObject(this, &UFEBuildingComponent::BuildPieceEntries));
}

void UFEBuildingComponent::CloseBuildMenu()
{
    if (!bIsMenuOpen) return;
    bIsMenuOpen = false;
    
    GetViewModel()->SetMenuOpen(false);
    UpdateUIMode();
}

void UFEBuildingComponent::SelectMenuTab(EFEBuildMenuTab Tab)
{
    MenuTab = Tab;
    // 정의 에셋은 메뉴를 열 때 MenuLoadHandle 로 이미 로드돼 있다 → 다시 로드하지 않고 목록만 다시 채운다
    BuildPieceEntries();
}

void UFEBuildingComponent::BuildPieceEntries()
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
        Entry->SetSelected(PieceId == SelectedPieceId);
        Entries.Add(Entry);
    }
    GetViewModel()->SetPieceEntries(Entries);
    GetViewModel()->SetMenuTabTitle(GetMenuTabTitle(MenuTab));
}

void UFEBuildingComponent::OpenSupplyPanel(AFEBuildPiece* Piece)
{
    if (Piece == nullptr || Piece == SupplyTarget)
    {
        CloseSupplyPanel();
        return;
    }
    CloseSupplyPanel();

    SupplyTarget = Piece;
    SupplyTarget->OnSupplyChangedNative.AddUObject(this, &UFEBuildingComponent::RefreshSupplyRows);
    SupplyTarget->OnDestroyed.AddDynamic(this, &UFEBuildingComponent::HandleSupplyTargetDestroyed);

    RebuildSupplyRows();
    GetViewModel()->SetSupplyPanelOpen(true);
    RefreshSupplyRows();
    UpdateUIMode();

    // [넣기] 활성은 소지품을 알아야 한다. 소지품은 리플리케이트되지 않으므로 스냅샷을 받는다 (보관함 패널과 같은 경로)
    ServerRequestInventory();
}

void UFEBuildingComponent::CloseSupplyPanel()
{
    if (SupplyTarget)
    {
        SupplyTarget->OnSupplyChangedNative.RemoveAll(this);
        SupplyTarget->OnDestroyed.RemoveDynamic(this, &UFEBuildingComponent::HandleSupplyTargetDestroyed);
        SupplyTarget = nullptr;
    }
    SupplyRowsDefinition = nullptr;
    bSupplyRowsLocked = false;
    
    GetViewModel()->SetSupplyPanelOpen(false);
    GetViewModel()->SetSupplyRows(TArray<UObject*>());
    UpdateUIMode();
}

void UFEBuildingComponent::OpenStoragePanel(AFEBuildStorage* Storage)
{
    if (Storage == nullptr || Storage == StorageTarget)
    {
        CloseStoragePanel();
        return;
    }
    CloseStoragePanel();

    StorageTarget = Storage;
    StorageTarget->OnContentsChangedNative.AddUObject(this, &UFEBuildingComponent::RefreshStorageSlots);
    StorageTarget->OnDestroyed.AddDynamic(this, &UFEBuildingComponent::HandleStorageTargetDestroyed);

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
    GetViewModel()->SetStoragePanelOpen(true);
    
    // 화로면 진행 바 줄을 보이고 점화·진행도 변화를 받는다
    AFEBuildFurnace* Furnace = Cast<AFEBuildFurnace>(Storage);
    if (Furnace)
    {
        Furnace->OnFurnaceStateChangedNative.AddUObject(this, &UFEBuildingComponent::RefreshFurnaceState);
    }
    GetViewModel()->SetFurnaceVisible(Furnace != nullptr);
    
    RefreshFurnaceState();
    RefreshStorageSlots();      // 보관함 내용만으로 먼저 그린다
    ServerRequestInventory();   // 소지품이 도착하면 ClientReceiveInventory 가 다시 그린다
    UpdateUIMode();
}

void UFEBuildingComponent::CloseStoragePanel()
{
    if (StorageTarget)
    {
        StorageTarget->OnContentsChangedNative.RemoveAll(this);
        StorageTarget->OnDestroyed.RemoveDynamic(this, &UFEBuildingComponent::HandleStorageTargetDestroyed);
        if (AFEBuildFurnace* Furnace = Cast<AFEBuildFurnace>(StorageTarget))
        {
            Furnace->OnFurnaceStateChangedNative.RemoveAll(this);
        }
        StorageTarget = nullptr;
    }
    CarriedItems.Reset();
    GetViewModel()->SetStoragePanelOpen(false);
    GetViewModel()->SetStorageSlots(TArray<UObject*>());
    GetViewModel()->SetCarriedSlots(TArray<UObject*>());
    GetViewModel()->SetFurnaceVisible(false);
    UpdateUIMode();
}

void UFEBuildingComponent::StoreStack(FGameplayTag ItemTag, int32 Count)
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

void UFEBuildingComponent::TakeSlot(int32 SlotIndex, FGameplayTag ItemTag)
{
    if (StorageTarget && ItemTag.IsValid() && SlotIndex >= 0)
    {
        ServerTakeItem(StorageTarget, SlotIndex, ItemTag);
    }
}

void UFEBuildingComponent::RefreshStorageSlots()
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

void UFEBuildingComponent::HandleStorageTargetDestroyed(AActor* DestroyedActor)
{
    CloseStoragePanel();
}

void UFEBuildingComponent::ShowNotice(const FText& Text)
{
    GetViewModel()->SetNotice(Text);

    // 2초 뒤 지운다. 연달아 오면 타이머가 처음부터 다시 돈다
    GetWorld()->GetTimerManager().SetTimer(NoticeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        GetViewModel()->SetNotice(FText::GetEmpty());
    }), 2.f, false);
}

void UFEBuildingComponent::UpdateBuildModeState()
{
    // 1) State.Building 태그. 빌드 모드 중엔 회전(E)과 상호작용(E)이 같은 키라 상호작용 컴포넌트가 이 태그를 보고 스스로 꺼진다.
    //    로컬 loose 태그라 다른 기계에는 보이지 않는다 — 서버 판정에 쓰면 안 된다.
    UAbilitySystemComponent* ASC = bIsInBuildMode
        ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner())
        : BuildModeTagOwner.Get();
    if (ASC != nullptr)
    {
        ASC->SetLooseGameplayTagCount(FEBuildingTags::State_Building, bIsInBuildMode ? 1 : 0);
    }
    BuildModeTagOwner = bIsInBuildMode ? ASC : nullptr;

    // 2) 빌드 모드 전용 IMC. 전투 IMC(우선순위 20)보다 높게 넣어 같은 키(좌·우클릭)를 가져온다 → 빌드 모드 중엔 공격이 나가지 않는다.
    //    뺄 때는 넣었던 서브시스템을 쓴다. 폰이 사라지는 EndPlay 에선 컨트롤러를 이미 못 찾을 수 있다.
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = BuildModeInputOwner.Get();
    if (bIsInBuildMode)
    {
        const APawn* Pawn = Cast<APawn>(GetOwner());
        const APlayerController* PlayerController = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
        InputSubsystem = PlayerController
            ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer())
            : nullptr;
    }

    // 소프트 참조라 로드 전이면 null. BeginPlay 의 비동기 로드가 끝나면 이 함수가 다시 불려 그때 넣는다
    UInputMappingContext* MappingContext = BuildModeMappingContext.Get();
    if (BuildModeMappingContext.IsNull())
    {
        if (bIsInBuildMode)
        {
            UE_LOG(LogFEBuilding, Warning, TEXT("BuildModeMappingContext is not set on %s. Place/Cancel/Rotate keys will not work."), *GetNameSafe(GetOwner()));
        }
    }
    else if (InputSubsystem != nullptr && MappingContext != nullptr)
    {
        if (bIsInBuildMode)
        {
            InputSubsystem->AddMappingContext(MappingContext, BuildModeMappingPriority);
        }
        else
        {
            InputSubsystem->RemoveMappingContext(MappingContext);
        }
        UE_LOG(LogFEBuilding, Log, TEXT("Build mode input %s (priority %d)"), bIsInBuildMode ? TEXT("added") : TEXT("removed"), BuildModeMappingPriority);
    }
    BuildModeInputOwner = bIsInBuildMode ? InputSubsystem : nullptr;
}

void UFEBuildingComponent::RefreshFurnaceState()
{
    if (const AFEBuildFurnace* Furnace = Cast<AFEBuildFurnace>(StorageTarget))
    {
        GetViewModel()->SetFurnaceState(Furnace->IsLit(), Furnace->GetProcessPercent() / 100.f);
    }
}

void UFEBuildingComponent::ToggleFurnaceLit()
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

bool UFEBuildingComponent::IsInBuildMode() const
{
    return bIsInBuildMode;
}

void UFEBuildingComponent::SupplyItem(FGameplayTag ItemTag)
{
    if (SupplyTarget && ItemTag.IsValid())
    {
        ServerSupplyItem(SupplyTarget, ItemTag);
    }
}

UFEBuildingViewModel* UFEBuildingComponent::GetViewModel()
{
    if (ViewModel == nullptr)
    {
        ViewModel = NewObject<UFEBuildingViewModel>(this);
        ViewModel->Component = this;
    }
    return ViewModel;
}

void UFEBuildingComponent::RefreshSupplyRows()
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

void UFEBuildingComponent::RebuildSupplyRows()
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

int32 UFEBuildingComponent::GetCarriedCount(FGameplayTag ItemTag) const
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

void UFEBuildingComponent::HandleSupplyTargetDestroyed(AActor* DestroyedActor)
{
    CloseSupplyPanel();
}

void UFEBuildingComponent::UpdateUIMode()
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

void UFEBuildingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bIsInBuildMode)
    {
        UpdatePreview();
    }
}

void UFEBuildingComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetNetMode() != NM_DedicatedServer)
    {
        // 망치 조준 HUD. 데디케이티드 서버는 화면이 없다. 원격 폰은 UpdateAimedPiece 가 걸러낸다
        // (상호작용 프롬프트와 같은 10Hz 폴링: "무엇을 조준하는지"는 카메라가 돌 때마다 바뀌는데 이벤트가 없다)
        GetWorld()->GetTimerManager().SetTimer(AimTimer, this, &UFEBuildingComponent::UpdateAimedPiece, 0.1f, true);

        // 빌드 모드 IMC. 로드가 끝났을 때 이미 빌드 모드면 그때 추가된다 (UpdateBuildModeState 는 여러 번 불러도 안전)
        if (!BuildModeMappingContext.IsNull())
        {
            BuildModeMappingLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
                BuildModeMappingContext.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &UFEBuildingComponent::UpdateBuildModeState));
        }
    }
}

void UFEBuildingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 빌드 모드 중 폰이 사라지면 PlayerState 의 ASC 에 태그가 남아 다음 폰의 상호작용이 막힌다
    bIsInBuildMode = false;
    UpdateBuildModeState();
    DestroyPreview();
    CloseSupplyPanel();
    CloseStoragePanel();
    GetWorld()->GetTimerManager().ClearTimer(AimTimer);
    Super::EndPlay(EndPlayReason);
}

void UFEBuildingComponent::UpdatePreview()
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FEBuild_UpdatePreview);
    
    if (SelectedPiece == nullptr) return; // 에셋 로드 중

    const bool bNeedsNewGhost = PreviewActor == nullptr || PreviewActor->GetDefinition() != SelectedPiece;
    if (bNeedsNewGhost)
    {
        DestroyPreview();
        PreviewActor = GetWorld()->SpawnActorDeferred<AFEBuildPiece>(
            ResolvePieceClass(SelectedPiece), FTransform::Identity, GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        PreviewActor->InitializePiece(SelectedPiece, EFEBuildPieceState::Preview);
        PreviewActor->FinishSpawning(FTransform::Identity);
        bIsPreviewValid = true; // InitializePiece 가 "유효" 고스트 머티리얼을 적용한 상태
        LastValidationTime = -1.0; // 피스가 바뀌었으면 바로 검증
    }

    if (!ComputePlacement(PreviewLocation, PreviewYawStep))
    {
        PreviewActor->SetActorHiddenInGame(true);
        bIsPreviewValid = false;
        LastValidationTime = -1.0; // 다시 보일 때 같은 자리여도 캐시를 믿지 않는다
        return;
    }
    
    // 원격 클라의 위치는 FVector_NetQuantize 로 1cm 반올림돼 서버에 도착한다.
    // 클라도 같은 위치로 놓고 검증해야 벽에 붙이거나 가장자리에 걸칠 때 "초록인데 서버 거부"가 생기지 않는다.
    PreviewLocation = PreviewLocation.GridSnap(1.f);

    const FTransform PlacementTransform = MakePlacementTransform(PreviewLocation, PreviewYawStep);
    PreviewActor->SetActorHiddenInGame(false);
    PreviewActor->SetActorTransform(PlacementTransform);

    DrawDebugOverlays();

    // 고스트 위치는 매 프레임 따라가고(끊겨 보이면 바로 티가 남), 무거운 배치 검증은 위치·회전이 바뀌면 PreviewMovingValidateInterval 간격으로,
    // 그대로면 PreviewRevalidateInterval 주기로만 한다(다른 플레이어가 옆에 지은 경우). 클릭 순간은 ConfirmPlacement 가 다시 검증한다
    const double Now = GetWorld()->GetTimeSeconds();
    const double SinceValidation = Now - LastValidationTime;
    const bool bTransformChanged = !PreviewLocation.Equals(LastValidatedLocation, 0.1f) || PreviewYawStep != LastValidatedYawStep;
    const bool bShouldValidate = LastValidationTime < 0.0 || (bTransformChanged && SinceValidation >= PreviewMovingValidateInterval)
        || SinceValidation >= PreviewRevalidateInterval;
    if (!bShouldValidate) return;

    LastValidatedLocation = PreviewLocation;
    LastValidatedYawStep = PreviewYawStep;
    LastValidationTime = Now;

    FText Reason;
    const bool bIsValid = ValidatePlacement(GetWorld(), SelectedPiece, PlacementTransform, GetOwner(), &Reason);
    const FString ReasonString = bIsValid ? FString() : Reason.ToString();
    if (bIsValid != bIsPreviewValid || ReasonString != LastInvalidReason)
    {
        PreviewActor->SetPreviewValid(bIsValid);
        if (bIsValid)
        {
            UE_LOG(LogFEBuilding, Verbose, TEXT("Preview valid"));
        }
        else
        {
            UE_LOG(LogFEBuilding, Verbose, TEXT("Preview invalid: %s"), *ReasonString);
        }
    }
    LastInvalidReason = ReasonString;
    bIsPreviewValid = bIsValid;
}

void UFEBuildingComponent::DestroyPreview()
{
    if (PreviewActor)
    {
        PreviewActor->Destroy();
        PreviewActor = nullptr;
    }
    bIsPreviewValid = false;
}

void UFEBuildingComponent::DrawDebugOverlays() const
{
    const bool bDrawSockets = CVarFEDebugSockets.GetValueOnGameThread();
    const bool bShowSupport = CVarFEShowSupport.GetValueOnGameThread();
    if (!bDrawSockets && !bShowSupport) return;

    const UWorld* World = GetWorld();
    const float Radius = bShowSupport ? 2000.f : UFEBuildingSettings::Get()->SnapRadius * 2.f;
    TArray<AFEBuildPiece*> Nearby;
    UFEBuildingSubsystem::GatherNearbyPieces(World, PreviewLocation, Radius, GetOwner(), Nearby);

    for (const AFEBuildPiece* Piece : Nearby)
    {
        if (bDrawSockets)
        {
            for (const FFEBuildSocket& Socket : Piece->GetDefinition()->Sockets)
            {
                DrawSocket(World, Socket, Piece->GetActorTransform(), Socket.AcceptTypes.IsEmpty() ? FColor::Cyan : FColor::Yellow);
            }
        }
        if (bShowSupport)
        {
            const FString Text = FString::Printf(TEXT("D:%d B:%d"), Piece->GetDesignSupportDistance(), Piece->GetSupportDistance());
            const bool bIsBuilt = Piece->GetState() == EFEBuildPieceState::Built;
            DrawDebugString(World, Piece->GetActorLocation() + FVector(0.f, 0.f, 60.f), Text, nullptr, bIsBuilt ? FColor::White : FColor::Cyan, 0.f, true);
        }
    }

    const bool bCanDrawPreview = bDrawSockets && PreviewActor != nullptr && PreviewActor->GetDefinition() != nullptr;
    if (bCanDrawPreview)
    {
        for (const FFEBuildSocket& Socket : PreviewActor->GetDefinition()->Sockets)
        {
            DrawSocket(World, Socket, PreviewActor->GetActorTransform(), FColor::Green);
        }
    }
}

bool UFEBuildingComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (Pawn == nullptr) return false;

    if (const APlayerController* PlayerController = Pawn->GetController<APlayerController>())
    {
        PlayerController->GetPlayerViewPoint(OutLocation, OutRotation);
    }
    else
    {
        Pawn->GetActorEyesViewPoint(OutLocation, OutRotation);
    }
    return true;
}

bool UFEBuildingComponent::ComputePlacement(FVector& OutLocation, uint8& OutYawStep) const
{
    FVector ViewLocation;
    FRotator ViewRotation;
    if (!GetViewPoint(ViewLocation, ViewRotation)) return false;

    const APawn* Pawn = Cast<APawn>(GetOwner());

    const UFEBuildingSettings* Settings = UFEBuildingSettings::Get();
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Settings->MaxBuildDistance;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(FEBuildPlacement), false, Pawn);
    Params.AddIgnoredActor(PreviewActor);

    // 청사진은 Visibility 를 무시하므로 채널이 아니라 오브젝트 타입으로 트레이스한다.
    // 이래야 청사진 기둥·벽을 올려다볼 때 커서가 그 위에 멈춰 스냅 반경 안에 들어온다.
    FCollisionObjectQueryParams CursorTypes;
    CursorTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    CursorTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    CursorTypes.AddObjectTypesToQuery(ECC_FEBuildPiece);

    FHitResult Hit;
    const bool bHitSomething = GetWorld()->LineTraceSingleByObjectType(Hit, ViewLocation, TraceEnd, CursorTypes, Params);
    const FVector CursorPoint = bHitSomething ? Hit.ImpactPoint : TraceEnd;

    // 근처 구조물의 소켓에 맞출 수 있으면 스냅이 우선. 스냅 중에는 회전과 지면 높이가 소켓에서 결정된다.
    if (FindSnapPlacement(CursorPoint, OutLocation, OutYawStep)) return true;

    OutLocation = CursorPoint;

    // 피스는 플레이어 방향을 RotationStep 단위로 따라가고, 거기에 Q/E 수동 오프셋을 더한다.
    const float PlayerYaw = Pawn->GetActorRotation().Yaw;
    OutYawStep = YawToStep(PlayerYaw + YawStepOffset * Settings->RotationStepDeg);
    
    // 칸 배치 피스(함정): 평평한 구조물 위면 그 판의 칸, 맨땅이면 근처 함정의 칸에 맞춘다. 높이는 아래 표면 맞춤이 정한다
    if (SelectedPiece != nullptr && SelectedPiece->GridCellSize > 0.f)
    {
        SnapToGrid(GetWorld(), SelectedPiece, bHitSomething ? &Hit : nullptr, YawStepOffset, Pawn, OutLocation, OutYawStep);
    }

    // 표면 배치 피스는 4코너 중 가장 높은 표면에 맞춰 올린다. 완만한 경사에서는 낮은 쪽이 떠서(다리) 파묻히지 않는다.
    const UStaticMesh* StaticMesh = SelectedPiece ? SelectedPiece->Mesh.Get() : nullptr;
    const bool bRaiseToSurface = StaticMesh != nullptr && SelectedPiece->bCanPlaceOnGround;
    if (bRaiseToSurface)
    {
        const FBox LocalBounds = StaticMesh->GetBoundingBox();
        const FTransform Guess = MakePlacementTransform(OutLocation, OutYawStep);
        FVector Corners[4];
        GetBottomCorners(LocalBounds, Corners);

        float HighestOriginZ = OutLocation.Z;
        for (const FVector& Corner : Corners)
        {
            FHitResult SurfaceHit;
            if (TraceSurface(GetWorld(), Guess.TransformPosition(Corner), SelectedPiece->MaxGroundHeightDelta, Pawn, SelectedPiece->bCanPlaceOnStructure, SurfaceHit))
            {
                HighestOriginZ = FMath::Max(HighestOriginZ, SurfaceHit.ImpactPoint.Z - LocalBounds.Min.Z);
            }
        }
        OutLocation.Z = HighestOriginZ;
    }
    return true;
}

bool UFEBuildingComponent::FindSnapPlacement(const FVector& CursorPoint, FVector& OutLocation, uint8& OutYawStep) const
{
    if (SelectedPiece == nullptr || SelectedPiece->Sockets.Num() == 0) return false;

    TArray<AFEBuildPiece*> Nearby;
    UFEBuildingSubsystem::GatherNearbyPieces(GetWorld(), CursorPoint, UFEBuildingSettings::Get()->SnapRadius, GetOwner(), Nearby);
    if (Nearby.Num() == 0) return false;

    // 타입이 맞는 (프리뷰 소켓, 대상 소켓) 쌍 중 대상 소켓이 커서에 가장 가까운 것을 고른다.
    float BestDistSq = TNumericLimits<float>::Max();
    FTransform BestTransform;
    bool bFound = false;
    
    const AFEBuildPiece* BestTarget = nullptr;
    FGameplayTag BestTargetSocketType;
    FGameplayTag BestPieceSocketType;

    for (const AFEBuildPiece* Target : Nearby)
    {
        const FTransform TargetXf = Target->GetActorTransform();
        for (const FFEBuildSocket& TargetSocket : Target->GetDefinition()->Sockets)
        {
            const FTransform TargetSocketWorld = TargetSocket.LocalTransform * TargetXf;
            const float DistSq = FVector::DistSquared(TargetSocketWorld.GetLocation(), CursorPoint);
            if (DistSq >= BestDistSq) continue;
            
            // 이 대상 소켓에 붙을 수 있는 프리뷰 소켓이 여럿이면 회전 입력(Q/E)으로 고른다
            // (삼각 벽 좌우 반전, 계단 내려오기/올라가기). 하나뿐이면 지금처럼 그 소켓
            TArray<const FFEBuildSocket*, TInlineAllocator<4>> Candidates;
            for (const FFEBuildSocket& PieceSocket : SelectedPiece->Sockets)
            {
                if (PieceSocket.AcceptTypes.HasTag(TargetSocket.Type))
                {
                    Candidates.Add(&PieceSocket);
                }
            }
            if (Candidates.Num() == 0) continue;

            const FFEBuildSocket& PieceSocket = *Candidates[YawStepOffset % Candidates.Num()];

            BestDistSq = DistSq;
            BestTransform = PieceSocket.LocalTransform.Inverse() * GetFlip180() * TargetSocketWorld;
            bFound = true;

            BestTarget = Target;
            BestTargetSocketType = TargetSocket.Type;
            BestPieceSocketType = PieceSocket.Type;
        }
    }

    if (!bFound) return false;
    
    if (CVarFEDebugSockets.GetValueOnGameThread())
    {
        const FString Description = FString::Printf(TEXT("%s <- %s.%s yaw %.0f"),
            *BestPieceSocketType.ToString(), *GetNameSafe(BestTarget), *BestTargetSocketType.ToString(), BestTransform.Rotator().Yaw);
        if (Description != LastSnapDescription)
        {
            LastSnapDescription = Description;
            UE_LOG(LogFEBuilding, Log, TEXT("Snap: %s"), *Description);
        }
    }
    
    OutLocation = BestTransform.GetLocation();
    OutYawStep = YawToStep(BestTransform.Rotator().Yaw);
    return true;
}

AFEBuildPiece* UFEBuildingComponent::FindPieceUnderCrosshair() const
{
    FVector ViewLocation;
    FRotator ViewRotation;
    if (!GetViewPoint(ViewLocation, ViewRotation)) return nullptr;

    // 청사진은 Visibility 채널을 무시하므로 오브젝트 타입으로 찾는다. 지형(WorldStatic)도 같이 맞혀서
    // 그게 먼저 맞으면(벽·바위 너머) 피스가 아니라 nullptr → 벽 너머 수리·업그레이드·철거 금지.
    // WorldDynamic 은 넣지 않는다: 오브젝트 타입 트레이스는 겹침 전용 트리거 볼륨(WorldDynamic)에서도 멈춘다
    FCollisionObjectQueryParams AimTypes;
    AimTypes.AddObjectTypesToQuery(ECC_FEBuildPiece);
    AimTypes.AddObjectTypesToQuery(ECC_WorldStatic);

    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * UFEBuildingSettings::Get()->MaxBuildDistance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(FEBuildAim), false, GetOwner());
    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, ViewLocation, TraceEnd, AimTypes, Params);
    return bHit ? Cast<AFEBuildPiece>(Hit.GetActor()) : nullptr;
}

bool UFEBuildingComponent::IsPieceInReach(const AFEBuildPiece* Piece) const
{
    const AActor* Owner = GetOwner();
    const bool bIsValidRequest = Owner != nullptr && Owner->HasAuthority() && Piece != nullptr && !Piece->IsActorBeingDestroyed();
    if (!bIsValidRequest) return false;

    const float MaxDistanceWithSlack = UFEBuildingSettings::Get()->MaxBuildDistance + ReachSlack;
    return FVector::Dist(Owner->GetActorLocation(), Piece->GetActorLocation()) <= MaxDistanceWithSlack;
}

FTransform UFEBuildingComponent::MakePlacementTransform(const FVector& Location, uint8 YawStep)
{
    const float Yaw = YawStep * UFEBuildingSettings::Get()->RotationStepDeg;
    return FTransform(FRotator(0.f, Yaw, 0.f), Location);
}

bool UFEBuildingComponent::ValidatePlacement(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FTransform& Transform, const AActor* Instigator, FText* OutReason)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FEBuild_ValidatePlacement);
    
    auto Fail = [OutReason](const FText& Why)
    {
        if (OutReason)
        {
            *OutReason = Why;
        }
        return false;
    };

    const UStaticMesh* StaticMesh = Piece ? Piece->Mesh.Get() : nullptr;
    const bool bHasLoadedPiece = World != nullptr && StaticMesh != nullptr;
    if (!bHasLoadedPiece)
    {
        return Fail(LOCTEXT("NotLoaded", "Piece assets are not loaded"));
    }

    const UFEBuildingSettings* Settings = UFEBuildingSettings::Get();

    const float MaxDistanceWithSlack = Settings->MaxBuildDistance + ReachSlack;
    if (Instigator && FVector::Dist(Instigator->GetActorLocation(), Transform.GetLocation()) > MaxDistanceWithSlack)
    {
        return Fail(LOCTEXT("TooFar", "Too far away"));
    }

    // 스냅 판정과 지지 예측이 같은 연결 목록을 쓴다
    TArray<AFEBuildPiece*> Connected;
    UFEBuildingSubsystem::FindConnectedPieces(World, Piece, Transform, Instigator, Connected);
    // 소켓 없는 피스(가구·방어시설)의 Connected 는 받침이라 "스냅"이 아니다 — 표면 규칙·전체 오버랩 검사를 그대로 받는다
    const bool bIsSnapped = !Piece->Sockets.IsEmpty() && Connected.Num() > 0;

    if (Piece->bRequiresSnap && !bIsSnapped)
    {
        return Fail(LOCTEXT("NeedsSnap", "Must be placed on a structure"));
    }

    // 지지 예측: 청사진 포함 설계 그래프 기준. anchor(IsAnchorAt — 토대·가구, 지면에 닿은 계단)는 0
    const bool bIsAnchor = UFEBuildingSubsystem::IsAnchorAt(World, Piece, Transform, Instigator);
    const uint8 PredictedDistance = UFEBuildingSubsystem::PredictSupportDistance(Piece, bIsAnchor, Connected, false);
    if (!UFEBuildingSubsystem::IsSupported(Piece, PredictedDistance))
    {
        return Fail(LOCTEXT("NoSupport", "Not enough support"));
    }

    const FBox LocalBounds = StaticMesh->GetBoundingBox();
    
    if (Piece->bRequiresFoundationBelow)
    {
        // 피스 중심 아래 기둥 모양 영역(FoundationSearchDepth)에 anchor(토대)가 있어야 한다. 2층 천장은 1층 천장을 지나 토대를 찾는다.
        const FVector Center = Transform.TransformPosition(LocalBounds.GetCenter());
        const FVector ColumnCenter = Center - FVector(0.f, 0.f, FoundationSearchDepth * 0.5f);
        TArray<FOverlapResult> Below;
        FCollisionQueryParams ColumnParams(SCENE_QUERY_STAT(FEBuildFoundationBelow), false, Instigator);
        World->OverlapMultiByObjectType(Below, ColumnCenter, FQuat::Identity, FCollisionObjectQueryParams(ECC_FEBuildPiece), FCollisionShape::MakeBox(FVector(10.f, 10.f, FoundationSearchDepth * 0.5f)), ColumnParams);

        const bool bHasFoundation = Below.ContainsByPredicate([](const FOverlapResult& Overlap)
        {
            const AFEBuildPiece* Other = Cast<AFEBuildPiece>(Overlap.GetActor());
            return Other && Other->IsAnchor();
        });
        if (!bHasFoundation)
        {
            return Fail(LOCTEXT("NoFoundationBelow", "Needs a foundation below"));
        }
    }

    // 표면 규칙은 스냅되지 않은 표면 배치에만 적용.
    const bool bUseSurfaceRules = Piece->bCanPlaceOnGround && !bIsSnapped;
    if (bUseSurfaceRules)
    {
        const float Slack = Piece->MaxGroundHeightDelta;

        // 지형 전용 피스(토대)는 구조물 위에 놓을 수 없다. 가구(bCanPlaceOnStructure)는 허용.
        if (!Piece->bCanPlaceOnStructure)
        {
            const FVector SlackOffset(0.f, 0.f, Slack);
            FCollisionQueryParams StructureParams(SCENE_QUERY_STAT(FEBuildStructure), false, Instigator);

            // 지형 배치 피스는 지형 위에만 놓인다. 구조물 위에 토대를 쌓는 것을 막는다.
            const FVector BottomCenter = Transform.TransformPosition(FVector(LocalBounds.GetCenter().X, LocalBounds.GetCenter().Y, LocalBounds.Min.Z));
            FHitResult StructureHit;
            const bool bOnStructure = World->LineTraceSingleByObjectType(StructureHit, BottomCenter + SlackOffset, BottomCenter - SlackOffset, FCollisionObjectQueryParams(ECC_FEBuildPiece), StructureParams);
            if (bOnStructure)
            {
                return Fail(LOCTEXT("OnStructure", "Cannot build on a structure"));
            }
        }

        if (Piece->MinSupportCoverage > 0.f)
        {
            // 바닥 4×4 지점 중 MinSupportCoverage(함정 80%) 이상이 표면에 닿아야 한다 — 토대 가장자리에 반쯤 걸치거나 구멍 위에 놓이지 않게
            const float Coverage = GetSupportCoverage(World, LocalBounds, Transform, Slack, Instigator, Piece->bCanPlaceOnStructure);
            if (Coverage < Piece->MinSupportCoverage)
            {
                return Fail(FText::Format(LOCTEXT("NotEnoughSurface", "Not enough surface below ({0}%)"), FText::AsNumber(FMath::RoundToInt(Coverage * 100.f))));
            }
        }
        else
        {
            // 4코너 모두 MaxGroundHeightDelta 안에 표면이 있어야 한다.
            FVector Corners[4];
            GetBottomCorners(LocalBounds, Corners);
            for (const FVector& Corner : Corners)
            {
                FHitResult SurfaceHit;
                if (!TraceSurface(World, Transform.TransformPosition(Corner), Slack, Instigator, Piece->bCanPlaceOnStructure, SurfaceHit))
                {
                    return Fail(LOCTEXT("Uneven", "Ground is too uneven"));
                }
            }
        }
    }

    // 소켓 없는 피스(가구·함정)는 맞닿기만 허용(1cm). 구조물은 코너 겹침·소켓 어긋남 허용(22cm)
    const float SideInset = Piece->Sockets.IsEmpty() ? FreeSideClearance : SideClearance;
    const FVector HalfExtent = LocalBounds.GetExtent();
    const FVector Extent(
        FMath::Max(1.f, HalfExtent.X - SideInset),
        FMath::Max(1.f, HalfExtent.Y - SideInset),
        FMath::Max(1.f, HalfExtent.Z - FaceClearance));
    const FVector Center = Transform.TransformPosition(LocalBounds.GetCenter());

    // 소켓 배치(bIsSnapped)는 코너 기둥+벽처럼 구조물끼리 겹치는 게 정상이라 BuildPiece 를 오버랩에서 뺀다.
    // 대신 같은 자리 중복만 따로 막는다. 지형 배치(토대·가구)는 구조물과 겹치면 안 되므로 그대로.
    if (bIsSnapped && Piece->DuplicateCoreScale <= 0.f)
    {
        // 심 상자를 쓸 수 없는 모양(직각 삼각 벽)은 같은 소켓 자리 — 피벗이 같은 곳 — 에 이미 피스가 있으면 중복으로 본다.
        // 반대로 뒤집은 삼각 벽, 같은 천장 가장자리의 벽·난간도 여기서 걸린다. 받치는 벽은 피벗이 아래라 걸리지 않는다
        TArray<AFEBuildPiece*> Nearby;
        UFEBuildingSubsystem::GatherNearbyPieces(World, Transform.GetLocation(), 50.f, Instigator, Nearby);
        const bool bIsOccupied = Nearby.ContainsByPredicate([&Transform](const AFEBuildPiece* Other)
        {
            return Other->GetActorLocation().Equals(Transform.GetLocation(), 5.f);
        });
        if (bIsOccupied)
        {
            return Fail(LOCTEXT("Duplicate", "Already placed here"));
        }
    }
    else if (bIsSnapped)
    {
        // 소켓 배치는 구조물끼리의 접촉·코너 겹침이 정상이므로 전체 상자로는 검사하지 않는다.
        // 대신 바운드를 DuplicateCoreScale(기본 40%) 로 줄인 "심" 상자가 다른 구조물과 겹치면 같은 자리/포개기로 보고 거부한다.
        // 코너 기둥(40cm)의 심은 벽 두께(20cm) 안으로 들어오지 않는다. 바운드가 모양보다 큰 피스(삼각 벽)는 DA 에서 줄인다.
        const FVector CoreExtent = LocalBounds.GetExtent() * Piece->DuplicateCoreScale;
        const FVector CoreCenter = Transform.TransformPosition(LocalBounds.GetCenter());
        FCollisionQueryParams CoreParams(SCENE_QUERY_STAT(FEBuildDuplicate), false, Instigator);
        TArray<FOverlapResult> CoreOverlaps;
        World->OverlapMultiByObjectType(
            CoreOverlaps, CoreCenter, Transform.GetRotation(), FCollisionObjectQueryParams(ECC_FEBuildPiece), FCollisionShape::MakeBox(CoreExtent), CoreParams);
        if (CoreOverlaps.Num() > 0)
        {
            // 진단: 무엇과 겹쳤는지 (log LogFEBuilding Verbose)
            UE_LOG(LogFEBuilding, Verbose, TEXT("Duplicate check hit %s (%s)"),
                *GetNameSafe(CoreOverlaps[0].GetActor()), *GetNameSafe(CoreOverlaps[0].GetComponent()));
            return Fail(LOCTEXT("Duplicate", "Already placed here"));
        }
    }

    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);                     // 다른 플레이어의 3인칭 메시(CharacterMesh 프로필)
    ObjectTypes.AddObjectTypesToQuery(FECollisionChannels::Player);  // 플레이어 캡슐 — Pawn 타입이 아니다
    ObjectTypes.AddObjectTypesToQuery(FECollisionChannels::Enemy);
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    if (!bIsSnapped)
    {
        ObjectTypes.AddObjectTypesToQuery(ECC_FEBuildPiece);
    }
    FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(FEBuildOverlap), false, Instigator);

    const bool bIsBlocked = World->OverlapAnyTestByObjectType(Center, Transform.GetRotation(), ObjectTypes, FCollisionShape::MakeBox(Extent), OverlapParams);
    if (bIsBlocked)
    {
        return Fail(LOCTEXT("Blocked", "Blocked by something"));
    }

    return true;
}

bool UFEBuildingComponent::ServerPlacePiece_Validate(FPrimaryAssetId InPieceId, FVector_NetQuantize Location, uint8 YawStep)
{
    // 정직한 클라이언트가 절대 보낼 수 없는 값만 거부한다(거부 = 킥). 일반적인 실패는 _Implementation 에서 로그만 남긴다.
    const bool bIsBuildPiece = InPieceId.PrimaryAssetType == UFEBuildPieceDefinition::AssetType;
    const bool bIsYawInRange = YawStep < UFEBuildingSettings::Get()->GetYawStepCount();
    return bIsBuildPiece && bIsYawInRange;
}

void UFEBuildingComponent::ServerPlacePiece_Implementation(FPrimaryAssetId InPieceId, FVector_NetQuantize Location, uint8 YawStep)
{
    // 청사진은 재료 없이 놓이므로 배치 자체가 공짜다. 조작된 클라이언트의 요청 폭주를 서버에서 끊는다.
    // ponytail: 플레이어당 미완성 청사진 개수 상한은 소유권(철거 권한) 규칙이 정해질 때 함께 추가
    const double Now = GetWorld()->GetTimeSeconds();
    if (LastPlaceRequestTime >= 0.0 && Now - LastPlaceRequestTime < MinPlaceInterval)
    {
        UE_LOG(LogFEBuilding, Verbose, TEXT("%s: placement request throttled"), *GetNameSafe(GetOwner()));
        return;
    }
    LastPlaceRequestTime = Now;
    
    UAssetManager& AssetManager = UAssetManager::Get();
    if (!AssetManager.GetPrimaryAssetPath(InPieceId).IsValid())
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("%s requested unknown piece %s"), *GetNameSafe(GetOwner()), *InPieceId.ToString());
        return;
    }

    // 이미 로드된 에셋이면 핸들이 null 로 오고 델리게이트는 즉시 실행된다.
    const TArray<FName> Bundles = { UFEBuildPieceDefinition::RuntimeBundle };
    TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAsset(
        InPieceId, Bundles, FStreamableDelegate::CreateUObject(this, &UFEBuildingComponent::HandleServerAssetsLoaded, InPieceId, FVector(Location), YawStep));

    if (Handle.IsValid())
    {
        PendingServerLoads.Add(Handle);
    }
}

void UFEBuildingComponent::HandleServerAssetsLoaded(FPrimaryAssetId LoadedPieceId, FVector Location, uint8 YawStep)
{
    PendingServerLoads.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle)
    {
        return !Handle.IsValid() || Handle->HasLoadCompleted();
    });

    AActor* Owner = GetOwner();
    if (Owner == nullptr || !Owner->HasAuthority()) return; // [Server Only]

    const UFEBuildPieceDefinition* Piece = Cast<UFEBuildPieceDefinition>(UAssetManager::Get().GetPrimaryAssetObject(LoadedPieceId));
    const FTransform PlacementTransform = MakePlacementTransform(Location, YawStep);

    FText Reason;
    if (!ValidatePlacement(GetWorld(), Piece, PlacementTransform, Owner, &Reason))
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("%s: placement of %s rejected: %s"), *GetNameSafe(Owner), *LoadedPieceId.ToString(), *Reason.ToString());
        return;
    }

    const EFEBuildPieceState InitialState = CVarFEInstantBuild.GetValueOnGameThread() ? EFEBuildPieceState::Built : EFEBuildPieceState::Blueprint;

    AFEBuildPiece* Spawned = GetWorld()->SpawnActorDeferred<AFEBuildPiece>(
        ResolvePieceClass(Piece), PlacementTransform, nullptr, Cast<APawn>(Owner), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Spawned->InitializePiece(Piece, InitialState);
    Spawned->FinishSpawning(PlacementTransform);
    
    UE_LOG(LogFEBuilding, Log, TEXT("%s placed %s (%s)"), *GetNameSafe(Owner), *Spawned->GetName(), *LoadedPieceId.ToString());
}

bool UFEBuildingComponent::ServerDemolishPiece_Validate(AFEBuildPiece* Piece)
{
    return Piece != nullptr;
}

void UFEBuildingComponent::ServerDemolishPiece_Implementation(AFEBuildPiece* Piece)
{
    if (!IsPieceInReach(Piece)) return;

    // 클라 검사와 서버 사이에 다른 플레이어가 아이템을 넣었을 수 있다. 판정은 서버 값으로.
    FText Reason;
    if (!Piece->CanDemolish(Reason))
    {
        UE_LOG(LogFEBuilding, Log, TEXT("Demolish refused on server: %s (%s)"), *Piece->GetName(), *Reason.ToString());
        return;
    }

    // 철거 권한(지은 사람/팀) 검사 없음. 소유권 규칙이 회의에서 정해지면 여기에 추가.
    Piece->Demolish(UFEBuildingSubsystem::FindInventoryProvider(GetOwner()));
}

bool UFEBuildingComponent::ServerRepairPiece_Validate(AFEBuildPiece* Piece)
{
    return true; // nullptr = 수리 대상 없이 휘두르기만
}

void UFEBuildingComponent::ServerRepairPiece_Implementation(AFEBuildPiece* Piece)
{
    if (!ServerSwingTool(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick)) return;
    if (!IsPieceInReach(Piece)) return; // nullptr 포함 (빈 곳에 휘두르기)

    IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner());
    if (Inventory == nullptr)
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("%s has no IFEBuildInventoryProvider; cannot repair"), *GetNameSafe(GetOwner()));
        return;
    }

    FText Reason;
    if (!Piece->TryRepair(*Inventory, Reason))
    {
        UE_LOG(LogFEBuilding, Log, TEXT("Repair rejected: %s (%s)"), *Piece->GetName(), *Reason.ToString());
        ClientShowNotice(Reason);
    }
}

bool UFEBuildingComponent::ServerUpgradePiece_Validate(AFEBuildPiece* Piece)
{
    return true; // nullptr = 대상 없이 휘두르기만
}

void UFEBuildingComponent::ServerUpgradePiece_Implementation(AFEBuildPiece* Piece)
{
    if (!ServerSwingTool(FallenEraCombatGameplayTags::Ability_Input_Combat_RightClick)) return;
    if (!IsPieceInReach(Piece)) return;

    IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner());
    if (Inventory == nullptr)
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("%s has no IFEBuildInventoryProvider; cannot upgrade"), *GetNameSafe(GetOwner()));
        return;
    }

    // 철거 권한처럼 업그레이드 권한도 아직 없음 (누구나). 소유권 규칙이 정해지면 여기에 추가
    FText Reason;
    if (!Piece->TryUpgrade(*Inventory, Reason))
    {
        UE_LOG(LogFEBuilding, Log, TEXT("Upgrade rejected: %s (%s)"), *Piece->GetName(), *Reason.ToString());
        ClientShowNotice(Reason);
    }
}

bool UFEBuildingComponent::IsHoldingRepairTool() const
{
    // ponytail: 아이템 식별이 FPrimaryAssetId 로 바뀌면 태그 대신 아이템 ID 로 비교
    const AActor* Owner = GetOwner();
    const UFE_EquipmentComponent* Equipment = Owner ? Owner->FindComponentByClass<UFE_EquipmentComponent>() : nullptr;
    const UFE_WeaponItemData* Weapon = Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
    return Weapon != nullptr && Weapon->HasItemTag(FEBuildingTags::Item_Tool_Hammer);
}

UAnimMontage* UFEBuildingComponent::GetToolMontage(FGameplayTag InputTag) const
{
    const AActor* Owner = GetOwner();
    const UFE_EquipmentComponent* Equipment = Owner ? Owner->FindComponentByClass<UFE_EquipmentComponent>() : nullptr;
    const UFE_WeaponAttackData* Attack = Equipment ? Equipment->GetCurrentAttackForInputTag(InputTag) : nullptr;
    return Attack ? Attack->AttackMontage.Get() : nullptr;
}

bool UFEBuildingComponent::TrySwingTool(FGameplayTag InputTag)
{
    // 연타 방지 (수리·업그레이드 공통 간격). 휘두르기는 대상이 없어도 한다
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastRepairClickTime < UFEBuildingSettings::Get()->RepairInterval) return false;
    LastRepairClickTime = Now;

    // 원격 클라는 자기 휘두르기를 바로 재생한다 — HT 의 MulticastPlayAttackMontage 가 원격 소유 클라를 건너뛰기 때문.
    // 호스트는 서버 쪽 멀티캐스트에서 한 번 재생되므로 여기서 재생하지 않는다 (중복 방지)
    if (!GetOwner()->HasAuthority())
    {
        AFallenEraCharacter* Character = Cast<AFallenEraCharacter>(GetOwner());
        UAnimMontage* Montage = GetToolMontage(InputTag);
        if (Character && Montage)
        {
            Character->PlayAttackMontage(Montage);
        }
    }
    return true;
}

bool UFEBuildingComponent::ServerSwingTool(FGameplayTag InputTag)
{
    // 속도 제한이 가장 먼저 — 도구 없이 보내는 요청도 거절 로그·안내 RPC 를 간격당 한 번만 만든다
    if (!ConsumeRequestSlot(NextRepairAllowedTime, GetWorld()->GetTimeSeconds(), UFEBuildingSettings::Get()->RepairInterval))
    {
        return false;
    }

    // GA 는 LocalOnly 라 클라에서만 돈다. 지금 망치를 들고 있는지는 서버가 장비 상태로 다시 확인한다
    if (!IsHoldingRepairTool())
    {
        const FText Reason = LOCTEXT("RepairNoTool", "수리 도구를 들고 있어야 합니다");
        UE_LOG(LogFEBuilding, Log, TEXT("Tool use rejected: %s"), *Reason.ToString());
        ClientShowNotice(Reason);
        return false;
    }

    // 다른 플레이어에게 휘두르기 표시. 원격 소유 클라는 HT 멀티캐스트가 건너뛴다 (이미 로컬 재생)
    AFallenEraCharacter* Character = Cast<AFallenEraCharacter>(GetOwner());
    if (UAnimMontage* Montage = GetToolMontage(InputTag); Character && Montage)
    {
        Character->MulticastPlayAttackMontage(Montage);
    }
    return true;
}

void UFEBuildingComponent::UpdateAimedPiece()
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

    // 망치를 들지 않았으면 트레이스도 하지 않는다
    const AFEBuildPiece* Piece = IsHoldingRepairTool() ? FindPieceUnderCrosshair() : nullptr;
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

void UFEBuildingComponent::ClientShowNotice_Implementation(const FText& Text)
{
    ShowNotice(Text);
}

bool UFEBuildingComponent::ServerSupplyItem_Validate(AFEBuildPiece* Piece, FGameplayTag ItemTag)
{
    return Piece != nullptr && ItemTag.IsValid();
}

void UFEBuildingComponent::ServerSupplyItem_Implementation(AFEBuildPiece* Piece, FGameplayTag ItemTag)
{
    if (!IsPieceInReach(Piece)) return;

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

bool UFEBuildingComponent::ServerStoreItem_Validate(AFEBuildStorage* Storage, FGameplayTag ItemTag, int32 Count)
{
    return Storage != nullptr && ItemTag.IsValid() && Count > 0;
}

void UFEBuildingComponent::ServerStoreItem_Implementation(AFEBuildStorage* Storage, FGameplayTag ItemTag, int32 Count)
{
    IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner());
    if (!IsPieceInReach(Storage) || Inventory == nullptr) return;

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

bool UFEBuildingComponent::ServerTakeItem_Validate(AFEBuildStorage* Storage, int32 SlotIndex, FGameplayTag ItemTag)
{
    return Storage != nullptr && SlotIndex >= 0 && ItemTag.IsValid();
}

void UFEBuildingComponent::ServerTakeItem_Implementation(AFEBuildStorage* Storage, int32 SlotIndex, FGameplayTag ItemTag)
{
    IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner());
    if (!IsPieceInReach(Storage) || Inventory == nullptr) return;

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

bool UFEBuildingComponent::ServerSetFurnaceLit_Validate(AFEBuildFurnace* Furnace, bool bLit)
{
    return Furnace != nullptr;
}

void UFEBuildingComponent::ServerSetFurnaceLit_Implementation(AFEBuildFurnace* Furnace, bool bLit)
{
    if (IsPieceInReach(Furnace))
    {
        Furnace->SetLit(bLit); // 켤 때 CanIgnite 를 서버 값으로 다시 검사한다
    }
}

bool UFEBuildingComponent::ServerRequestInventory_Validate()
{
    return true;
}

void UFEBuildingComponent::ServerRequestInventory_Implementation()
{
    SendInventorySnapshot();
}

void UFEBuildingComponent::SendInventorySnapshot()
{
    TArray<FFEBuildItemCost> Snapshot;
    if (IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner()))
    {
        Inventory->GetItems(Snapshot);
    }
    ClientReceiveInventory(Snapshot);
}

void UFEBuildingComponent::ClientReceiveInventory_Implementation(const TArray<FFEBuildItemCost>& InItems)
{
    CarriedItems = InItems;
    RefreshStorageSlots();
    RefreshSupplyRows(); // 투입 패널이 열려 있으면 [넣기] 활성 갱신 (없으면 바로 빠져나감)
}

#undef LOCTEXT_NAMESPACE

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFEBuildRequestRateLimitTest, "FallenEra.Building.RequestRateLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFEBuildRequestRateLimitTest::RunTest(const FString& Parameters)
{
    constexpr double Interval = 0.6;

    // 조작된 클라: 절반 간격(0.3초)으로 10초 동안 보내도 평균 간격(0.6초)만큼만 받는다 (처음 1회 여유)
    double NextAllowed = 0.0;
    int32 Accepted = 0;
    for (int32 Step = 0; Step < 34; ++Step)
    {
        Accepted += ConsumeRequestSlot(NextAllowed, Step * 0.3, Interval) ? 1 : 0;
    }
    TestTrue(FString::Printf(TEXT("spam held to the average interval (accepted %d)"), Accepted), Accepted >= 16 && Accepted <= 18);

    // 정직한 클라: 0.6초 간격에 ±0.1초 지터가 섞여도 전부 받는다
    NextAllowed = 0.0;
    const double Jitter[] = { 0.0, 0.1, -0.1, 0.05, -0.1, 0.1, -0.1 };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Jitter); ++Index)
    {
        TestTrue(FString::Printf(TEXT("click %d accepted"), Index), ConsumeRequestSlot(NextAllowed, Index * Interval + Jitter[Index], Interval));
    }
    return true;
}
#endif