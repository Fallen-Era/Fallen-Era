// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "UObject/PrimaryAssetId.h"
#include "FEBuildingTypes.h"
#include "FEBuildingComponent.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class AFEBuildPiece;
class UFEBuildPieceDefinition;
struct FStreamableHandle;

/**
 * 플레이어 캐릭터에 부착. 빌드 모드: 로컬 고스트 프리뷰, 회전, 검증, 그 다음 Server RPC 로 배치 요청.
 * 철거(X)와 망치 수리·업그레이드도 여기서 서버에 요청한다. 서버는 모든 요청을 재검증한다.
 * 입력은 GA_Build_* 어빌리티(Ability.Input.Build.*)가 아래 BlueprintCallable 함수를 호출하는 방식으로 들어오며, 이 컴포넌트는 입력을 직접 바인딩하지 않는다.
 * HUD·메뉴·패널은 UFEBuildHudComponent 가 맡는다. 이 컴포넌트는 HUD 를 모르고, 상태가 바뀌면 OnBuildStateChangedNative 로 알리기만 한다
 * (안내 문구는 UFEBuildingSubsystem::ShowLocalNotice).
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

    /** [Client Only] 조준한 피스 철거 요청 (X). 청사진 100% / 완성품 RefundRate 환불. */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void DemolishPiece();
    
    /** [Client Only] 조준한 완성 피스 수리 요청. 망치 무기의 GA_Build_Repair(좌클릭)가 호출한다 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void RepairPiece();
    
    /** [Client Only] 조준한 완성 피스를 다음 재질로 업그레이드 요청. 망치 무기의 GA_Build_Upgrade(우클릭)가 호출한다 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void UpgradePiece();

    /** 카메라 앞 MaxBuildDistance 안에서 조준 중인 피스. 지형·월드 물체가 먼저 맞으면(벽 너머) nullptr. 프리뷰 고스트는 콜리전이 없어 잡히지 않는다. 디버그 명령도 사용 */
    AFEBuildPiece* FindPieceUnderCrosshair() const;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    bool IsInBuildMode() const;
    
    /** 지금 고른 피스 (HUD 메뉴 선택 표시용). 빌드 모드 밖에서도 마지막 선택을 유지한다 */
    FPrimaryAssetId GetSelectedPieceId() const { return SelectedPieceId; }

    /** SelectedPieceId 의 정의. Runtime 번들 로드 전이면 nullptr */
    const UFEBuildPieceDefinition* GetSelectedPiece() const { return SelectedPiece.Get(); }

    /** 지금 망치를 장착하고 있다 (ASC 태그 Equipment.Weapon.Hammer). 서버 판정과 클라 HUD(조준 체력 줄)가 같이 쓴다 */
    bool IsHoldingRepairTool() const;

    /** [Server Only] Requester 의 요청을 Piece 에 처리해도 되는지 (서버 권한 + 유효성 + 거리). 건설·HUD 컴포넌트의 서버 RPC 가 같이 쓴다 */
    static bool IsPieceInReach(const AActor* Requester, const AFEBuildPiece* Piece);

    /** [Native] [Client Only] 빌드 모드 진입·해제, 고른 피스 로드 완료. HUD 가 구독해 표시를 맞춘다 */
    FSimpleMulticastDelegate OnBuildStateChangedNative;

    /** 클라이언트(고스트 색상)와 서버(스폰 전)가 공유하는 배치 규칙. 정의의 Runtime 번들이 로드되어 있어야 한다. */
    static bool ValidatePlacement(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FTransform& Transform, const AActor* Instigator, FText* OutReason = nullptr);

    /** 패킹된 배치 정보에서 월드 트랜스폼을 만든다. 클라와 서버가 같은 함수를 써서 결과가 정확히 일치한다. */
    static FTransform MakePlacementTransform(const FVector& Location, uint8 YawStep);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    /** 빌드 모드 진입 시 선택되는 피스. 빌드 메뉴가 이를 대체한다. */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building", meta = (AllowedTypes = "BuildPiece"))
    FPrimaryAssetId DefaultPieceId;
    
    /** 빌드 모드 중에만 추가되는 입력 (배치·취소·회전·철거). 전투 IMC(우선순위 20)보다 높아야 좌·우클릭을 가져온다.
     *  소프트 참조: BeginPlay 에서 비동기 로드. 로드 전에 빌드 모드에 들어가면 로드가 끝날 때 추가된다 */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Input")
    TSoftObjectPtr<UInputMappingContext> BuildModeMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Input")
    int32 BuildModeMappingPriority = 30;

    /** [Server RPC] Location 은 정수 cm 로 양자화, YawStep 은 0..GetYawStepCount()-1 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerPlacePiece(FPrimaryAssetId InPieceId, FVector_NetQuantize Location, uint8 YawStep);

    /** [Server RPC] Piece 철거, 환불은 요청자의 인벤토리로 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerDemolishPiece(AFEBuildPiece* Piece);
    
    /** [Server RPC] 망치 장착 + 거리 확인 후 Piece 수리. 실패 사유는 ClientShowNotice 로 돌려준다 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerRepairPiece(AFEBuildPiece* Piece);
    
    /** [Server RPC] 망치 장착 + 거리 확인 후 Piece 업그레이드. nullptr = 대상 없이 휘두르기만 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpgradePiece(AFEBuildPiece* Piece);

    /** [Client RPC] 서버만 아는 실패 사유(재료 부족·도구 없음)를 요청자 HUD 에 띄운다 */
    UFUNCTION(Client, Reliable)
    void ClientShowNotice(const FText& Text);

private:
    void HandlePreviewAssetsLoaded(FPrimaryAssetId LoadedPieceId);
    void HandleServerAssetsLoaded(FPrimaryAssetId LoadedPieceId, FVector Location, uint8 YawStep);

    void UpdatePreview();
    void DestroyPreview();
    void DrawDebugOverlays() const;

    /** 오너의 시점(카메라). 오너가 Pawn 이 아니면 false */
    bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

    /** 카메라 트레이스 -> 패킹된 배치 정보. 오너가 Pawn 이 아니면 false. */
    bool ComputePlacement(FVector& OutLocation, uint8& OutYawStep) const;

    /** 커서 근처 구조물의 소켓에 맞춘 배치. 후보가 없으면 false. */
    bool FindSnapPlacement(const FVector& CursorPoint, FVector& OutLocation, uint8& OutYawStep) const;

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

    /** [Server Only] 배치 요청으로 진행 중인 로드들. */
    TArray<TSharedPtr<FStreamableHandle>> PendingServerLoads;
    
    /** 디버그: 마지막 스냅 쌍 설명. 바뀔 때만 로그 */
    mutable FString LastSnapDescription;
    
    FString LastInvalidReason;
    
    /** [Client Only] 마지막으로 검증한 배치 위치·회전. 같으면 검증(오버랩 여러 번)을 건너뛴다 */
    FVector LastValidatedLocation = FVector::ZeroVector;
    uint8 LastValidatedYawStep = 0;

    /** [Client Only] 마지막 검증 시각(월드 초). 음수면 다음 프레임에 무조건 검증 */
    double LastValidationTime = -1.0;

    /** [Server Only] 마지막 배치 요청 시각. 배치 속도 제한용 */
    double LastPlaceRequestTime = -1.0;

    /** 들고 있는 도구에서 InputTag(좌/우클릭)에 해당하는 공격 몽타주 (망치 DA 의 AttackActions). 없으면 nullptr */
    UAnimMontage* GetToolMontage(FGameplayTag InputTag) const;

    /** [Client Only] 휘두르기 공통 처리: 간격 확인 + 원격 클라 로컬 재생. 휘둘렀으면 true */
    bool TrySwingTool(FGameplayTag InputTag);

    /** [Server Only] 휘두르기 공통 처리: 간격 확인(가장 먼저 — 도구 없는 요청도 속도 제한) → 망치 확인 → 다른 플레이어에게 몽타주 멀티캐스트. 계속 진행하면 true */
    bool ServerSwingTool(FGameplayTag InputTag);

    /** [Client Only] 마지막 수리 휘두르기 시각. RepairInterval 연타 방지 */
    double LastRepairClickTime = -1.0;
    
    /** [Server Only] 다음 수리·업그레이드 요청을 받을 수 있는 시각. 받을 때마다 RepairInterval 씩 누적한다
     *  → 지터로 조금 일찍 온 요청은 받되 평균 간격은 RepairInterval 을 넘지 못한다 */
    double NextRepairAllowedTime = 0.0;
    
    /** State.Building 태그와 빌드 모드 전용 입력(IMC)을 빌드 모드 여부에 맞춘다 */
    void UpdateBuildModeState();
    
    /** 태그를 건 ASC. 폰이 죽어 빙의가 풀려도 같은 ASC 에서 떼기 위해 기억한다 (ASC 는 PlayerState 소유) */
    TWeakObjectPtr<UAbilitySystemComponent> BuildModeTagOwner;

    /** IMC 를 넣은 로컬 플레이어 입력 서브시스템. 폰이 사라져 컨트롤러를 못 찾아도 같은 곳에서 빼기 위해 기억한다 */
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BuildModeInputOwner;
    
    /** BuildModeMappingContext 비동기 로드 핸들 */
    TSharedPtr<FStreamableHandle> BuildModeMappingLoadHandle;
};
