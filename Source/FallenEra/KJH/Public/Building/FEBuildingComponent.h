// Fallen Era 건설 시스템 (KJH)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "UObject/PrimaryAssetId.h"
#include "Building/FEBuildingTypes.h"
#include "FEBuildingComponent.generated.h"

class AFEBuildPiece;
class UFEBuildPieceDefinition;
struct FStreamableHandle;

/**
 * 플레이어 캐릭터에 부착. 빌드 모드: 로컬 고스트 프리뷰, 회전, 검증, 그 다음 Server RPC 로 배치 요청.
 * 서버는 스폰 전에 같은 규칙으로 재검증한다.
 * 입력은 GA_Build_* 어빌리티(Ability.Input.Build.*)가 아래 BlueprintCallable 함수를 호출하는 방식으로 들어오며,
 * 이 컴포넌트는 입력을 직접 바인딩하지 않는다.
 */
UCLASS(ClassGroup = (FallenEra), meta = (BlueprintSpawnableComponent))
class FALLENERA_API UFEBuildingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFEBuildingComponent();

    /** [Client Only] DefaultPieceId 로 빌드 모드에 진입하거나, 이미 진입 중이면 나간다. */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void ToggleBuildMode();

    /** [Client Only] 프리뷰할 피스 변경. 에셋은 비동기 로드되며 준비되면 고스트가 나타난다. */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void SelectPiece(FPrimaryAssetId InPieceId);

    /** [Client Only] +1 시계 방향 / -1 반시계 방향, RotationStepDeg 단위. */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void RotatePreview(int32 Direction);

    /** [Client Only] 현재 프리뷰가 유효하면 서버에 배치를 요청. 빌드 모드는 유지된다. */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void ConfirmPlacement();

    /** [Client Only] 빌드 모드를 나가고 고스트를 제거. */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void CancelBuild();

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    bool IsInBuildMode() const;

    /**
     * 클라이언트(고스트 색상)와 서버(스폰 전)가 공유하는 배치 규칙.
     * 정의의 Runtime 번들이 로드되어 있어야 한다. Instigator 는 오버랩 검사에서 제외된다.
     */
    static bool ValidatePlacement(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FTransform& Transform, const AActor* Instigator, FText* OutReason = nullptr);

    /** 패킹된 배치 정보에서 월드 트랜스폼을 만든다. 클라와 서버가 같은 함수를 써서 결과가 정확히 일치한다. */
    static FTransform MakePlacementTransform(const FVector& Location, uint8 YawStep);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    /** 빌드 모드 진입 시 선택되는 피스. 빌드 메뉴(S5)가 이를 대체한다. */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building", meta = (AllowedTypes = "BuildPiece"))
    FPrimaryAssetId DefaultPieceId;

    /** [Server RPC] Location 은 정수 cm 로 양자화, YawStep 은 0..GetYawStepCount()-1 (15도 단위). */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerPlacePiece(FPrimaryAssetId InPieceId, FVector_NetQuantize Location, uint8 YawStep);

private:
    void HandlePreviewAssetsLoaded(FPrimaryAssetId LoadedPieceId);
    void HandleServerAssetsLoaded(FPrimaryAssetId LoadedPieceId, FVector Location, uint8 YawStep);

    void UpdatePreview();
    void DestroyPreview();

    /** 카메라 트레이스 -> 패킹된 배치 정보. 오너가 Pawn 이 아니면 false. */
    bool ComputePlacement(FVector& OutLocation, uint8& OutYawStep) const;

    bool bIsInBuildMode = false;
    bool bIsPreviewValid = false;
    int32 YawStepOffset = 0;

    FPrimaryAssetId SelectedPieceId;
    FVector PreviewLocation = FVector::ZeroVector;
    uint8 PreviewYawStep = 0;

    /** SelectedPieceId 의 Runtime 번들이 로드된 뒤 해석됨. */
    UPROPERTY(Transient)
    TObjectPtr<const UFEBuildPieceDefinition> SelectedPiece;

    UPROPERTY(Transient)
    TObjectPtr<AFEBuildPiece> PreviewActor;

    TSharedPtr<FStreamableHandle> PreviewLoadHandle;

    /** [Server Only] 배치 요청으로 진행 중인 로드들. 빠르게 연속 배치하면 여러 개가 겹칠 수 있다. */
    TArray<TSharedPtr<FStreamableHandle>> PendingServerLoads;
};
