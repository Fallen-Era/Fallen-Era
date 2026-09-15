// Fallen Era 건설 시스템 (KJH)

#include "Building/FEBuildingComponent.h"
#include "Building/FEBuildPiece.h"
#include "Building/FEBuildPieceDefinition.h"
#include "Building/FEBuildingSettings.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

// ponytail: S2 인벤토리 전까지는 배치 즉시 완성. 재료 투입이 생기면 기본값을 false 로 바꿀 것.
static TAutoConsoleVariable<bool> CVarFEInstantBuild(
    TEXT("fe.Build.InstantBuild"), true,
    TEXT("Place pieces as Built instead of Blueprint (skips material fill)."));

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
    
    /** 지형(WorldStatic/WorldDynamic)만 대상으로 아래로 트레이스. 구조물(BuildPiece)은 타입이 달라 지형으로 치지 않는다. */
    bool TraceGround(const UWorld* World, const FVector& Point, float Slack, const AActor* IgnoreActor, FHitResult& OutHit)
    {
        // 랜드스케이프는 WorldStatic, 템플릿 Floor 나 BlockAllDynamic 메시는 WorldDynamic 이라 둘 다 지면으로 본다.
        FCollisionObjectQueryParams GroundTypes;
        GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
        GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

        const FVector Offset(0.f, 0.f, Slack);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(FEBuildGround), false, IgnoreActor);
        return World->LineTraceSingleByObjectType(OutHit, Point + Offset, Point - Offset, GroundTypes, Params);
    }

    /** 바닥 4코너의 로컬 위치 (Z = 메시 하단) */
    void GetBottomCorners(const FBox& LocalBounds, FVector OutCorners[4])
    {
        OutCorners[0] = FVector(LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Min.Z);
        OutCorners[1] = FVector(LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Min.Z);
        OutCorners[2] = FVector(LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Min.Z);
        OutCorners[3] = FVector(LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Min.Z);
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
}

bool UFEBuildingComponent::IsInBuildMode() const
{
    return bIsInBuildMode;
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

    FText Reason;
    const bool bIsValid = ValidatePlacement(GetWorld(), SelectedPiece, PlacementTransform, GetOwner(), &Reason);
    if (bIsValid != bIsPreviewValid)
    {
        PreviewActor->SetPreviewValid(bIsValid); // 유효/무효가 바뀔 때만 머티리얼 교체
        if (bIsValid)
        {
            UE_LOG(LogFEBuilding, Log, TEXT("Preview valid"));
        }
        else
        {
            UE_LOG(LogFEBuilding, Log, TEXT("Preview invalid: %s"), *Reason.ToString());
        }
    }
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

bool UFEBuildingComponent::ComputePlacement(FVector& OutLocation, uint8& OutYawStep) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (Pawn == nullptr)
    {
        return false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    if (const APlayerController* PlayerController = Pawn->GetController<APlayerController>())
    {
        PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        Pawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const UFEBuildingSettings* Settings = UFEBuildingSettings::Get();
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Settings->MaxBuildDistance;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(FEBuildPlacement), false, Pawn);
    Params.AddIgnoredActor(PreviewActor);

    FHitResult Hit;
    const bool bHitSomething = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params);
    OutLocation = bHitSomething ? Hit.ImpactPoint : TraceEnd;

    // 피스는 플레이어 방향을 RotationStep 단위로 따라가고, 거기에 Q/E 수동 오프셋을 더한다.
    const float PlayerYaw = Pawn->GetActorRotation().Yaw;
    OutYawStep = YawToStep(PlayerYaw + YawStepOffset * Settings->RotationStepDeg);

    // 지형 배치 피스는 4코너 중 가장 높은 지면에 맞춰 올린다. 완만한 경사에서는 낮은 쪽이 떠서(다리) 지형에 파묻히지 않는다.
    // 가파른 경사는 ValidatePlacement 의 MaxGroundHeightDelta 검사가 거른다.
    const UStaticMesh* StaticMesh = SelectedPiece ? SelectedPiece->Mesh.Get() : nullptr;
    const bool bRaiseToGround = StaticMesh != nullptr && SelectedPiece->bCanPlaceOnGround;
    if (bRaiseToGround)
    {
        const FBox LocalBounds = StaticMesh->GetBoundingBox();
        const FTransform Guess = MakePlacementTransform(OutLocation, OutYawStep);
        FVector Corners[4];
        GetBottomCorners(LocalBounds, Corners);

        float HighestOriginZ = OutLocation.Z;
        for (const FVector& Corner : Corners)
        {
            FHitResult GroundHit;
            if (TraceGround(GetWorld(), Guess.TransformPosition(Corner), SelectedPiece->MaxGroundHeightDelta, Pawn, GroundHit))
            {
                HighestOriginZ = FMath::Max(HighestOriginZ, GroundHit.ImpactPoint.Z - LocalBounds.Min.Z);
            }
        }
        OutLocation.Z = HighestOriginZ;
    }
    return true;
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

    // 서버 측 정상성 검사. 클라이언트 트레이스가 이미 거리를 제한하므로 눈높이만큼 여유를 둔다.
    const float MaxDistanceWithSlack = Settings->MaxBuildDistance + 300.f;
    if (Instigator && FVector::Dist(Instigator->GetActorLocation(), Transform.GetLocation()) > MaxDistanceWithSlack)
    {
        return Fail(LOCTEXT("TooFar", "Too far away"));
    }

    if (Piece->bRequiresSnap)
    {
        // S3 에서 소켓 스냅이 추가된다. 그 전까지 스냅 전용 피스는 배치할 수 없다.
        return Fail(LOCTEXT("NeedsSnap", "Must be placed on a structure"));
    }

    const FBox LocalBounds = StaticMesh->GetBoundingBox();

    if (Piece->bCanPlaceOnGround)
    {
        const float Slack = Piece->MaxGroundHeightDelta;
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

        // 4코너 모두 MaxGroundHeightDelta 안에 지형이 있어야 한다. 절벽 밖으로 걸치거나 너무 가파르면 실패.
        FVector Corners[4];
        GetBottomCorners(LocalBounds, Corners);
        for (const FVector& Corner : Corners)
        {
            FHitResult GroundHit;
            if (!TraceGround(World, Transform.TransformPosition(Corner), Slack, Instigator, GroundHit))
            {
                return Fail(LOCTEXT("Uneven", "Ground is too uneven"));
            }
        }
    }

    // 옆면은 인접 피스와의 면 공유를 허용하도록 5% 줄이고,
    // 아랫면은 지형과 정확히 맞닿아 있으므로 GroundClearance 만큼 띄워서 지면 자체가 오버랩으로 잡히지 않게 한다.
    // 지형·스태틱 메시(WorldStatic)와 움직이는 물체(WorldDynamic)도 대상에 넣어 경사면이나 바위를 관통하는 배치를 막는다.
    const float GroundClearance = 2.f;
    const FVector HalfExtent = LocalBounds.GetExtent();
    const FVector Extent(HalfExtent.X * 0.95f, HalfExtent.Y * 0.95f, FMath::Max(1.f, HalfExtent.Z - GroundClearance * 0.5f));
    const FVector Center = Transform.TransformPosition(LocalBounds.GetCenter() + FVector(0.f, 0.f, GroundClearance * 0.5f));

    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_FEBuildPiece);
    ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
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

#undef LOCTEXT_NAMESPACE
