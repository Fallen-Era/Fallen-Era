// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "UObject/PrimaryAssetId.h"
#include "FEBuildingTypes.h"
#include "FEBuildingComponent.generated.h"

class UAbilitySystemComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class AFEBuildFurnace;
class AFEBuildStorage;
class UFEBuildingViewModel;
class AFEBuildPiece;
class UFEBuildPieceDefinition;
struct FStreamableHandle;

/**
 * 플레이어 캐릭터에 부착. 빌드 모드: 로컬 고스트 프리뷰, 회전, 검증, 그 다음 Server RPC 로 배치 요청.
 * 청사진 재료 투입(F)과 철거(X)도 여기서 서버에 요청한다. 서버는 모든 요청을 재검증한다.
 * 입력은 GA_Build_* 어빌리티(Ability.Input.Build.*)가 아래 BlueprintCallable 함수를 호출하는 방식으로 들어오며, 이 컴포넌트는 입력을 직접 바인딩하지 않는다.
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

    /** 카메라 앞 MaxBuildDistance 안에서 조준 중인 피스. 없으면 nullptr. 프리뷰 고스트는 콜리전이 없어 잡히지 않는다. 디버그 명령도 사용 */
    AFEBuildPiece* FindPieceUnderCrosshair() const;

    /** [Client Only] 빌드 메뉴 열기/닫기 (Tab). 빌드 모드 밖이면 진입하며 연다. */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void ToggleBuildMenu();

    /** [Client Only] 메뉴 닫기 (뷰모델 "닫기" 버튼, 피스 선택 후) */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void CloseBuildMenu();

    /** [Client Only] 청사진 재료 투입 패널 열기. 같은 피스면 닫는다. AFEBuildPiece::InteractLocal 이 호출 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void OpenSupplyPanel(AFEBuildPiece* Piece);

    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void CloseSupplyPanel();

    /** [Client Only] 투입 패널의 한 항목 "넣기" → 서버 요청 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void SupplyItem(FGameplayTag ItemTag);
    
    /** [Client Only] 저장고 패널 열기. 같은 저장고면 닫는다. AFEBuildStorage::InteractBuiltLocal 이 호출 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void OpenStoragePanel(AFEBuildStorage* Storage);

    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void CloseStoragePanel();

    /** [Client Only] 소지품 칸 클릭 → 이 종류를 Count 개 보관함으로 (칸·용량까지) */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void StoreStack(FGameplayTag ItemTag, int32 Count);

    /** [Client Only] 보관함 칸 클릭 → 그 칸을 통째로 내 인벤토리로 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void TakeSlot(int32 SlotIndex, FGameplayTag ItemTag);
    
    /** [Client Only] 화로 패널의 불 켜기/끄기 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building")
    void ToggleFurnaceLit();

    /** HUD 위젯이 바인딩할 뷰모델. 지연 생성 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    UFEBuildingViewModel* GetViewModel();

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    bool IsInBuildMode() const;

    /** 클라이언트(고스트 색상)와 서버(스폰 전)가 공유하는 배치 규칙. 정의의 Runtime 번들이 로드되어 있어야 한다. */
    static bool ValidatePlacement(const UWorld* World, const UFEBuildPieceDefinition* Piece, const FTransform& Transform, const AActor* Instigator, FText* OutReason = nullptr);

    /** 패킹된 배치 정보에서 월드 트랜스폼을 만든다. 클라와 서버가 같은 함수를 써서 결과가 정확히 일치한다. */
    static FTransform MakePlacementTransform(const FVector& Location, uint8 YawStep);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    /** 빌드 모드 진입 시 선택되는 피스. 빌드 메뉴가 이를 대체한다. */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building", meta = (AllowedTypes = "BuildPiece"))
    FPrimaryAssetId DefaultPieceId;
    
    /** 빌드 모드 중에만 추가되는 입력 (배치·취소·회전·철거). 전투 IMC(우선순위 20)보다 높아야 좌·우클릭을 가져온다 */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Input")
    TObjectPtr<UInputMappingContext> BuildModeMappingContext;

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

    /** [Client RPC] 서버만 아는 실패 사유(재료 부족·도구 없음)를 요청자 HUD 에 띄운다 */
    UFUNCTION(Client, Reliable)
    void ClientShowNotice(const FText& Text);
    
    /** [Server RPC] 요청자의 인벤토리에서 Piece 의 ItemTag 항목에 있는 만큼 투입 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerSupplyItem(AFEBuildPiece* Piece, FGameplayTag ItemTag);
    
    /** [Server RPC] 요청자의 인벤토리 → 보관함. Count 는 클라 표시 기준이며 서버가 실제 보유량으로 다시 자른다 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerStoreItem(AFEBuildStorage* Storage, FGameplayTag ItemTag, int32 Count);

    /** [Server RPC] 보관함 SlotIndex 칸 → 요청자의 인벤토리 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerTakeItem(AFEBuildStorage* Storage, int32 SlotIndex, FGameplayTag ItemTag);
    
    /** [Server RPC] 화로 점화/소화 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerSetFurnaceLit(AFEBuildFurnace* Furnace, bool bLit);

    /** [Server RPC] 패널을 열 때 소지품 목록 요청 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerRequestInventory();

    /** [Client RPC] 소지품 스냅샷. 인벤토리는 리플리케이트되지 않으므로 패널 표시용으로만 받는다 */
    UFUNCTION(Client, Reliable)
    void ClientReceiveInventory(const TArray<FFEBuildItemCost>& InItems);

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

    /** [Server Only] 이 피스에 대한 요청을 처리해도 되는지 (유효성 + 거리) */
    bool IsPieceInReach(const AFEBuildPiece* Piece) const;
    
    /** 메뉴/패널이 열린 동안 커서 표시 + UI 전용 입력. 둘 다 닫히면 게임 입력으로 복귀 */
    void UpdateUIMode();

    /** Asset Manager 의 BuildPiece 목록 → 엔트리 뷰모델. 정의 에셋 로드 완료 후 호출 */
    void BuildPieceEntries();

    /** SupplyTarget 의 현재 투입량으로 행 뷰모델 갱신. 다 찼으면 패널을 닫는다 */
    void RefreshSupplyRows();

    UFUNCTION()
    void HandleSupplyTargetDestroyed(AActor* DestroyedActor);
    
    /** 보관함 칸(내용만 갱신)과 소지품 칸(다시 생성)을 그린다 */
    void RefreshStorageSlots();
    
    /** 열린 패널이 화로면 진행 바·버튼 문구 갱신 */
    void RefreshFurnaceState();

    /** [Server Only] 현재 소지품을 요청자에게 보낸다 */
    void SendInventorySnapshot();
    
    /** [Client Only] HUD 에 안내 문구를 2초 띄운다 */
    void ShowNotice(const FText& Text);

    FTimerHandle NoticeTimer;

    UFUNCTION()
    void HandleStorageTargetDestroyed(AActor* DestroyedActor);

    bool bIsMenuOpen = false;

    UPROPERTY(Transient)
    TObjectPtr<UFEBuildingViewModel> ViewModel;

    /** 투입 패널이 보고 있는 청사진 */
    UPROPERTY(Transient)
    TObjectPtr<AFEBuildPiece> SupplyTarget;
    
    /** 저장고 패널이 보고 있는 저장고 */
    UPROPERTY(Transient)
    TObjectPtr<AFEBuildStorage> StorageTarget;

    /** [Client Only] 서버가 보내 준 소지품 스냅샷. 패널 표시에만 쓴다 (판정은 전부 서버) */
    TArray<FFEBuildItemCost> CarriedItems;

    TSharedPtr<FStreamableHandle> MenuLoadHandle;

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
    
    /** State.Building 태그와 빌드 모드 전용 입력(IMC)을 빌드 모드 여부에 맞춘다 */
    void UpdateBuildModeState();
    
    /** 태그를 건 ASC. 폰이 죽어 빙의가 풀려도 같은 ASC 에서 떼기 위해 기억한다 (ASC 는 PlayerState 소유) */
    TWeakObjectPtr<UAbilitySystemComponent> BuildModeTagOwner;

    /** IMC 를 넣은 로컬 플레이어 입력 서브시스템. 폰이 사라져 컨트롤러를 못 찾아도 같은 곳에서 빼기 위해 기억한다 */
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BuildModeInputOwner;
};
