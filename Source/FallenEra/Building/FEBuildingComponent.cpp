// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildingComponent.h"
#include "FEBuildInventoryProvider.h"
#include "FEBuildPiece.h"
#include "FEBuildPieceDefinition.h"
#include "FEBuildingSettings.h"
#include "FEBuildingSubsystem.h"
#include "FEBuildingViewModel.h"
#include "DrawDebugHelpers.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

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
     * bIncludeStructures 면 완성된 구조물(BuildPiece)도 표면으로 인정한다 (가구를 토대·천장 위에 놓을 때).
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

        const FVector Offset(0.f, 0.f, Slack);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(FEBuildGround), false, IgnoreActor);
        return World->LineTraceSingleByObjectType(OutHit, Point + Offset, Point - Offset, SurfaceTypes, Params);
    }

    /** 바닥 4코너의 로컬 위치 (Z = 메시 하단) */
    void GetBottomCorners(const FBox& LocalBounds, FVector OutCorners[4])
    {
        OutCorners[0] = FVector(LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Min.Z);
        OutCorners[1] = FVector(LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Min.Z);
        OutCorners[2] = FVector(LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Min.Z);
        OutCorners[3] = FVector(LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Min.Z);
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

    if (!InPieceId.IsValid())
    {
        return;
    }

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
    if (!bStillWanted)
    {
        return; // 로드 중에 다른 피스를 고르거나 빌드 모드를 나간 경우
    }
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
    const bool bCanPlace = bIsInBuildMode && bIsPreviewValid && SelectedPiece != nullptr;
    if (!bCanPlace)
    {
        return;
    }
    ServerPlacePiece(SelectedPieceId, PreviewLocation, PreviewYawStep);
}

void UFEBuildingComponent::CancelBuild()
{
    bIsInBuildMode = false;
    SetComponentTickEnabled(false);
    DestroyPreview();
    SelectedPiece = nullptr;
    PreviewLoadHandle.Reset();
    GetViewModel()->SetBuildMode(false);
    CloseBuildMenu();
}

void UFEBuildingComponent::DemolishPiece()
{
    if (AFEBuildPiece* Piece = FindPieceUnderCrosshair())
    {
        ServerDemolishPiece(Piece);
    }
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
    if (!bIsMenuOpen)
    {
        return;
    }
    bIsMenuOpen = false;
    GetViewModel()->SetMenuOpen(false);
    UpdateUIMode();
}

void UFEBuildingComponent::BuildPieceEntries()
{
    if (!bIsMenuOpen)
    {
        return; // 로드 중에 닫힘
    }

    TArray<FPrimaryAssetId> PieceIds;
    UAssetManager::Get().GetPrimaryAssetIdList(UFEBuildPieceDefinition::AssetType, PieceIds);
    PieceIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
    {
        return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName);
    });

    TArray<UObject*> Entries;
    for (const FPrimaryAssetId& PieceId : PieceIds)
    {
        const UFEBuildPieceDefinition* Definition = Cast<UFEBuildPieceDefinition>(UAssetManager::Get().GetPrimaryAssetObject(PieceId));
        const FText Name = Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromName(PieceId.PrimaryAssetName);

        UFEBuildPieceEntryViewModel* Entry = NewObject<UFEBuildPieceEntryViewModel>(GetViewModel());
        Entry->Initialize(this, PieceId, Name);
        Entry->SetSelected(PieceId == SelectedPieceId);
        Entries.Add(Entry);
    }
    GetViewModel()->SetPieceEntries(Entries);
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

    const UFEBuildPieceDefinition* Definition = Piece->GetDefinition();
    GetViewModel()->SetSupplyTitle(Definition ? Definition->DisplayName : FText::GetEmpty());

    TArray<UObject*> Rows;
    if (Definition)
    {
        for (const FFEBuildItemCost& Cost : Definition->RequiredItems)
        {
            UFESupplyRowViewModel* Row = NewObject<UFESupplyRowViewModel>(GetViewModel());
            Row->Initialize(this, Cost.ItemTag, UFEBuildingSettings::Get()->GetItemDisplayName(Cost.ItemTag));
            Rows.Add(Row);
        }
    }
    GetViewModel()->SetSupplyRows(Rows);
    GetViewModel()->SetSupplyPanelOpen(true);
    RefreshSupplyRows();
    UpdateUIMode();
}

void UFEBuildingComponent::CloseSupplyPanel()
{
    if (SupplyTarget)
    {
        SupplyTarget->OnSupplyChangedNative.RemoveAll(this);
        SupplyTarget->OnDestroyed.RemoveDynamic(this, &UFEBuildingComponent::HandleSupplyTargetDestroyed);
        SupplyTarget = nullptr;
    }
    GetViewModel()->SetSupplyPanelOpen(false);
    GetViewModel()->SetSupplyRows(TArray<UObject*>());
    UpdateUIMode();
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
    if (Definition == nullptr)
    {
        return;
    }
    if (SupplyTarget->IsFullySupplied())
    {
        CloseSupplyPanel(); // 다 찼다. 완성 여부는 지지 구조가 결정하므로 패널은 여기서 끝
        return;
    }

    const TArray<TObjectPtr<UObject>>& Rows = GetViewModel()->SupplyRows;
    for (int32 Index = 0; Index < Rows.Num() && Index < Definition->RequiredItems.Num(); ++Index)
    {
        if (UFESupplyRowViewModel* Row = Cast<UFESupplyRowViewModel>(Rows[Index]))
        {
            Row->SetCounts(SupplyTarget->GetSuppliedCount(Index), Definition->RequiredItems[Index].Count);
        }
    }
}

void UFEBuildingComponent::HandleSupplyTargetDestroyed(AActor* DestroyedActor)
{
    CloseSupplyPanel();
}

void UFEBuildingComponent::UpdateUIMode()
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    APlayerController* PlayerController = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
    if (PlayerController == nullptr || !PlayerController->IsLocalController())
    {
        return;
    }

    const bool bUIOpen = bIsMenuOpen || SupplyTarget != nullptr;
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

void UFEBuildingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyPreview();
    CloseSupplyPanel();
    Super::EndPlay(EndPlayReason);
}

void UFEBuildingComponent::UpdatePreview()
{
    if (SelectedPiece == nullptr)
    {
        return; // 에셋 로드 중
    }

    const bool bNeedsNewGhost = PreviewActor == nullptr || PreviewActor->GetDefinition() != SelectedPiece;
    if (bNeedsNewGhost)
    {
        DestroyPreview();
        PreviewActor = GetWorld()->SpawnActorDeferred<AFEBuildPiece>(
            ResolvePieceClass(SelectedPiece), FTransform::Identity, GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        PreviewActor->InitializePiece(SelectedPiece, EFEBuildPieceState::Preview);
        PreviewActor->FinishSpawning(FTransform::Identity);
        bIsPreviewValid = true; // InitializePiece 가 "유효" 고스트 머티리얼을 적용한 상태
    }

    if (!ComputePlacement(PreviewLocation, PreviewYawStep))
    {
        PreviewActor->SetActorHiddenInGame(true);
        bIsPreviewValid = false;
        return;
    }

    const FTransform PlacementTransform = MakePlacementTransform(PreviewLocation, PreviewYawStep);
    PreviewActor->SetActorHiddenInGame(false);
    PreviewActor->SetActorTransform(PlacementTransform);

    DrawDebugOverlays();

    FText Reason;
    const bool bIsValid = ValidatePlacement(GetWorld(), SelectedPiece, PlacementTransform, GetOwner(), &Reason);
    const FString ReasonString = bIsValid ? FString() : Reason.ToString();
    if (bIsValid != bIsPreviewValid || ReasonString != LastInvalidReason)
    {
        PreviewActor->SetPreviewValid(bIsValid);
        if (bIsValid)
        {
            UE_LOG(LogFEBuilding, Log, TEXT("Preview valid"));
        }
        else
        {
            UE_LOG(LogFEBuilding, Log, TEXT("Preview invalid: %s"), *ReasonString);
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
    if (!bDrawSockets && !bShowSupport)
    {
        return;
    }

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
    if (Pawn == nullptr)
    {
        return false;
    }

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
    if (!GetViewPoint(ViewLocation, ViewRotation))
    {
        return false;
    }
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
    if (FindSnapPlacement(CursorPoint, OutLocation, OutYawStep))
    {
        return true;
    }

    OutLocation = CursorPoint;

    // 피스는 플레이어 방향을 RotationStep 단위로 따라가고, 거기에 Q/E 수동 오프셋을 더한다.
    const float PlayerYaw = Pawn->GetActorRotation().Yaw;
    OutYawStep = YawToStep(PlayerYaw + YawStepOffset * Settings->RotationStepDeg);

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
    if (SelectedPiece == nullptr || SelectedPiece->Sockets.Num() == 0)
    {
        return false;
    }

    TArray<AFEBuildPiece*> Nearby;
    UFEBuildingSubsystem::GatherNearbyPieces(GetWorld(), CursorPoint, UFEBuildingSettings::Get()->SnapRadius, GetOwner(), Nearby);
    if (Nearby.Num() == 0)
    {
        return false;
    }

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
            if (DistSq >= BestDistSq)
            {
                continue;
            }
            for (const FFEBuildSocket& PieceSocket : SelectedPiece->Sockets)
            {
                if (!PieceSocket.AcceptTypes.HasTag(TargetSocket.Type))
                {
                    continue;
                }
                BestDistSq = DistSq;
                BestTransform = PieceSocket.LocalTransform.Inverse() * GetFlip180() * TargetSocketWorld;
                bFound = true;
                
                BestTarget = Target;
                BestTargetSocketType = TargetSocket.Type;
                BestPieceSocketType = PieceSocket.Type;
                
                break;
            }
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
    if (!GetViewPoint(ViewLocation, ViewRotation))
    {
        return nullptr;
    }

    // 청사진은 Visibility 채널을 무시하므로 오브젝트 타입(BuildPiece)으로 찾는다.
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * UFEBuildingSettings::Get()->MaxBuildDistance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(FEBuildAim), false, GetOwner());
    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, ViewLocation, TraceEnd, FCollisionObjectQueryParams(ECC_FEBuildPiece), Params);
    return bHit ? Cast<AFEBuildPiece>(Hit.GetActor()) : nullptr;
}

bool UFEBuildingComponent::IsPieceInReach(const AFEBuildPiece* Piece) const
{
    const AActor* Owner = GetOwner();
    const bool bIsValidRequest = Owner != nullptr && Owner->HasAuthority() && Piece != nullptr && !Piece->IsActorBeingDestroyed();
    if (!bIsValidRequest)
    {
        return false;
    }
    const float MaxDistanceWithSlack = UFEBuildingSettings::Get()->MaxBuildDistance + 300.f;
    return FVector::Dist(Owner->GetActorLocation(), Piece->GetActorLocation()) <= MaxDistanceWithSlack;
}

FTransform UFEBuildingComponent::MakePlacementTransform(const FVector& Location, uint8 YawStep)
{
    const float Yaw = YawStep * UFEBuildingSettings::Get()->RotationStepDeg;
    return FTransform(FRotator(0.f, Yaw, 0.f), Location);
}

bool UFEBuildingComponent::ValidatePlacement(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FTransform& Transform, const AActor* Instigator, FText* OutReason)
{
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

    const float MaxDistanceWithSlack = Settings->MaxBuildDistance + 300.f;
    if (Instigator && FVector::Dist(Instigator->GetActorLocation(), Transform.GetLocation()) > MaxDistanceWithSlack)
    {
        return Fail(LOCTEXT("TooFar", "Too far away"));
    }

    // 스냅 판정과 지지 예측이 같은 연결 목록을 쓴다
    TArray<AFEBuildPiece*> Connected;
    UFEBuildingSubsystem::FindConnectedPieces(World, Piece, Transform, Instigator, Connected);
    const bool bIsSnapped = Connected.Num() > 0;

    if (Piece->bRequiresSnap && !bIsSnapped)
    {
        return Fail(LOCTEXT("NeedsSnap", "Must be placed on a structure"));
    }

    // 지지 예측: 청사진 포함 설계 그래프 기준. anchor 는 항상 0.
    const uint8 PredictedDistance = UFEBuildingSubsystem::PredictSupportDistance(Piece, Connected, false);
    if (!UFEBuildingSubsystem::IsSupported(Piece, PredictedDistance))
    {
        return Fail(LOCTEXT("NoSupport", "Not enough support"));
    }

    const FBox LocalBounds = StaticMesh->GetBoundingBox();
    
    if (Piece->bRequiresFoundationBelow)
    {
        // 피스 중심 아래 기둥 모양 영역(2000cm)에 anchor(토대)가 있어야 한다. 2층 천장은 1층 천장을 지나 토대를 찾는다.
        const FVector Center = Transform.TransformPosition(LocalBounds.GetCenter());
        const FVector ColumnCenter = Center - FVector(0.f, 0.f, 1000.f);
        TArray<FOverlapResult> Below;
        FCollisionQueryParams ColumnParams(SCENE_QUERY_STAT(FEBuildFoundationBelow), false, Instigator);
        World->OverlapMultiByObjectType(Below, ColumnCenter, FQuat::Identity, FCollisionObjectQueryParams(ECC_FEBuildPiece), FCollisionShape::MakeBox(FVector(10.f, 10.f, 1000.f)), ColumnParams);

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

    // 옆면은 SideClearance 만큼 줄인다(코너 직각 벽·이웃 토대 소켓 어긋남 허용). 같은 자리 중복은 중심이 겹쳐 여전히 잡힌다.
    // 위아래는 FaceClearance 만큼 띄운다(지면 접촉, 천장 아래 끼우기).
    const float SideClearance = 22.f;
    const float FaceClearance = 2.f;
    const FVector HalfExtent = LocalBounds.GetExtent();
    const FVector Extent(
        FMath::Max(1.f, HalfExtent.X - SideClearance),
        FMath::Max(1.f, HalfExtent.Y - SideClearance),
        FMath::Max(1.f, HalfExtent.Z - FaceClearance));
    const FVector Center = Transform.TransformPosition(LocalBounds.GetCenter());

    // 소켓 배치(bIsSnapped)는 코너 기둥+벽처럼 구조물끼리 겹치는 게 정상이라 BuildPiece 를 오버랩에서 뺀다.
    // 대신 같은 자리 중복만 따로 막는다. 지형 배치(토대·가구)는 구조물과 겹치면 안 되므로 그대로.
    if (bIsSnapped)
    {
        // 소켓 배치는 구조물끼리의 접촉·코너 겹침이 정상이므로 전체 상자로는 검사하지 않는다.
        // 대신 바운드를 40% 로 줄인 "심" 상자가 다른 구조물과 겹치면 같은 자리/포개기로 보고 거부한다.
        // 코너 기둥(40cm)의 심은 벽 두께(20cm) 안으로 들어오지 않는다.
        const FVector CoreExtent = LocalBounds.GetExtent() * 0.4f;
        const FVector CoreCenter = Transform.TransformPosition(LocalBounds.GetCenter());
        FCollisionQueryParams CoreParams(SCENE_QUERY_STAT(FEBuildDuplicate), false, Instigator);
        const bool bOverlapsStructure = World->OverlapAnyTestByObjectType(
            CoreCenter, Transform.GetRotation(), FCollisionObjectQueryParams(ECC_FEBuildPiece), FCollisionShape::MakeBox(CoreExtent), CoreParams);
        if (bOverlapsStructure)
        {
            return Fail(LOCTEXT("Duplicate", "Already placed here"));
        }
    }

    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
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
    if (Owner == nullptr || !Owner->HasAuthority())
    {
        return; // [Server Only]
    }

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
}

bool UFEBuildingComponent::ServerDemolishPiece_Validate(AFEBuildPiece* Piece)
{
    return Piece != nullptr;
}

void UFEBuildingComponent::ServerDemolishPiece_Implementation(AFEBuildPiece* Piece)
{
    if (!IsPieceInReach(Piece))
    {
        return;
    }
    // ponytail: 철거 권한(지은 사람/팀) 검사 없음. 소유권 규칙이 회의에서 정해지면 여기에 추가.
    Piece->Demolish(UFEBuildingSubsystem::FindInventoryProvider(GetOwner()));
}

bool UFEBuildingComponent::ServerSupplyItem_Validate(AFEBuildPiece* Piece, FGameplayTag ItemTag)
{
    return Piece != nullptr && ItemTag.IsValid();
}

void UFEBuildingComponent::ServerSupplyItem_Implementation(AFEBuildPiece* Piece, FGameplayTag ItemTag)
{
    if (!IsPieceInReach(Piece))
    {
        return;
    }
    IFEBuildInventoryProvider* Inventory = UFEBuildingSubsystem::FindInventoryProvider(GetOwner());
    if (Inventory == nullptr)
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("%s has no IFEBuildInventoryProvider; cannot supply"), *GetNameSafe(GetOwner()));
        return;
    }
    Piece->TrySupply(*Inventory, ItemTag);
}

#undef LOCTEXT_NAMESPACE