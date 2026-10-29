// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "FEBuildingTypes.h"
#include "FEBuildHudComponent.generated.h"

enum class EFEBuildMenuTab : uint8;
class AFEBuildBallista;
class AFEBuildFurnace;
class AFEBuildPiece;
class AFEBuildStorage;
class UFEBuildingComponent;
class UFEBuildingViewModel;
class UFEBuildPieceDefinition;
struct FStreamableHandle;

/**
 * 플레이어 캐릭터에 부착. 건설 HUD 담당: 뷰모델(WBP_BuildHud), 빌드 메뉴(Tab), 재료 투입·보관함·화로·발리스타 탄약 패널,
 * 안내 문구, 망치 조준 체력 줄, 발리스타 탑승 HUD. 패널 조작(투입·넣기·꺼내기·점화·소지품)은 이 컴포넌트의 Server RPC 로 가고 서버가 다시 검사한다.
 * 빌드 모드·배치·철거·수리는 UFEBuildingComponent 가 맡는다. 이 컴포넌트는 그쪽 상태를 읽고 OnBuildStateChangedNative 알림을 받기만 한다
 * (건설 컴포넌트는 HUD 를 모른다). 피스는 상호작용(E) 때 이 컴포넌트의 패널을 연다.
 */
UCLASS(ClassGroup = (FallenEra), meta = (BlueprintSpawnableComponent))
class FALLENERA_API UFEBuildHudComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFEBuildHudComponent();

    /** [Client Only] 빌드 메뉴 열기/닫기 (Tab, GA_Build_Menu). 빌드 모드 밖이면 진입하며 연다.
     *  발리스타 탑승 중이거나 빌드 모드 밖에서 완성 발리스타를 조준하고 있으면 메뉴 대신 탄약 패널 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void ToggleBuildMenu();

    /** [Client Only] 메뉴 닫기 (뷰모델 "닫기" 버튼, 피스 선택 후, 빌드 모드 해제) */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void CloseBuildMenu();

    /** [Client Only] 빌드 메뉴 탭 변경 → 그 탭의 피스만 목록에 다시 채운다 */
    void SelectMenuTab(EFEBuildMenuTab Tab);

    /** [Client Only] 메뉴 엔트리 클릭 → 건설 컴포넌트에 피스 선택을 넘기고 메뉴를 닫는다 */
    void SelectPieceFromMenu(FPrimaryAssetId PieceId);

    /** [Client Only] 청사진 재료 투입 패널 열기. 같은 피스면 닫는다. AFEBuildPiece::InteractLocal 이 호출 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void OpenSupplyPanel(AFEBuildPiece* Piece);

    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void CloseSupplyPanel();

    /** [Client Only] 투입 패널의 한 항목 "넣기" → 서버 요청 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void SupplyItem(FGameplayTag ItemTag);

    /** [Client Only] 보관함 패널 열기 (발리스타면 탄약 패널). 같은 보관함이면 닫는다. AFEBuildStorage::InteractBuiltLocal 과 Tab(탄약)이 호출 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void OpenStoragePanel(AFEBuildStorage* Storage);

    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void CloseStoragePanel();

    /** [Client Only] 소지품 칸 클릭 → 이 종류를 Count 개 보관함으로 (칸·용량까지) */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void StoreStack(FGameplayTag ItemTag, int32 Count);

    /** [Client Only] 보관함 칸 클릭 → 그 칸을 통째로 내 인벤토리로 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void TakeSlot(int32 SlotIndex, FGameplayTag ItemTag);

    /** [Client Only] 화로 패널의 불 켜기/끄기 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|UI")
    void ToggleFurnaceLit();

    /** HUD 위젯이 바인딩할 뷰모델. 지연 생성. 컨트롤러(HT BP)가 빙의 때 WBP_BuildHud 에 넣는다 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Building|UI")
    UFEBuildingViewModel* GetViewModel();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
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

    /** [Client RPC] 서버만 아는 투입 실패 사유(다른 플레이어가 먼저 재질을 정함 등)를 요청자 HUD 에 띄운다 */
    UFUNCTION(Client, Reliable)
    void ClientShowNotice(const FText& Text);

private:
    /** 건설 컴포넌트 알림: 빌드 모드 표시·선택 피스 이름·메뉴 선택 표시를 맞추고, 빌드 모드가 끝났으면 메뉴를 닫는다 */
    void HandleBuildStateChanged();

    /** 메뉴/패널이 열린 동안 커서 표시 + UI 전용 입력. 전부 닫히면 게임 입력으로 복귀 */
    void UpdateUIMode();

    /** Asset Manager 의 BuildPiece 목록 → 엔트리 뷰모델. 정의 에셋 로드 완료 후 호출 */
    void BuildPieceEntries();

    /** SupplyTarget 의 현재 투입량·소지품으로 줄 갱신. 정의나 잠금 상태가 바뀌었으면 줄을 다시 만든다. 다 찼으면 패널을 닫는다 */
    void RefreshSupplyRows();

    /** 투입 패널 줄을 새로 만든다. 재질 미정 = 사슬 전체(선택지), 잠김 = 현재 정의의 재료만 */
    void RebuildSupplyRows();

    /** 클라 소지품 스냅샷(CarriedItems)에서 ItemTag 개수 */
    int32 GetCarriedCount(FGameplayTag ItemTag) const;

    UFUNCTION()
    void HandleSupplyTargetDestroyed(AActor* DestroyedActor);

    /** 보관함 칸(내용만 갱신)과 소지품 칸(다시 생성)을 그린다 */
    void RefreshStorageSlots();

    /** 열린 패널이 화로면 진행 바·버튼 문구 갱신 */
    void RefreshFurnaceState();

    UFUNCTION()
    void HandleStorageTargetDestroyed(AActor* DestroyedActor);

    /** [Server Only] 현재 소지품을 요청자에게 보낸다 */
    void SendInventorySnapshot();

    /** [Client Only] HUD 에 안내 문구를 2초 띄운다. 다른 컴포넌트·액터는 UFEBuildingSubsystem::ShowLocalNotice 로 보낸다 */
    void ShowNotice(const FText& Text);

    /** [Client Only] 0.1초 타이머: 망치를 들고 있으면 조준한 완성 피스의 체력·수리 비용을, 탑승 중이면 발리스타 탄약을 뷰모델에 넣는다 */
    void UpdateAimedPiece();

    /** [Client Only] 탑승 HUD(탄약 수·장전 중). 내가 탄 발리스타를 찾아 값이 바뀔 때만 뷰모델을 갱신한다.
     *  LastHudAmmo = -1 이면 HUD 가 꺼져 있고 다음에 무조건 그린다 */
    void UpdateBallistaHud();

    /** Tab 이 탄약 UI 를 열 발리스타: 탑승 중이면 그 발리스타, 아니면(빌드 모드 밖) 조준한 완성 발리스타. 없으면 nullptr */
    AFEBuildBallista* FindAmmoPanelTarget() const;

    /** 같은 액터의 건설 컴포넌트. 빌드 모드·선택 피스·조준·망치 판정을 읽는다. 없으면 메뉴·조준 HUD 만 꺼지고 패널은 동작한다 */
    TWeakObjectPtr<UFEBuildingComponent> BuildingComponent;

    UPROPERTY(Transient)
    TObjectPtr<UFEBuildingViewModel> ViewModel;

    bool bIsMenuOpen = false;

    /** 마지막으로 연 메뉴 탭. 다음에 메뉴를 열어도 유지 */
    EFEBuildMenuTab MenuTab{};

    TSharedPtr<FStreamableHandle> MenuLoadHandle;

    /** 투입 패널이 보고 있는 청사진 */
    UPROPERTY(Transient)
    TObjectPtr<AFEBuildPiece> SupplyTarget;

    /** 지금 투입 패널 줄을 만든 기준. 정의(재질)나 잠금 상태가 바뀌면 줄을 다시 만든다 */
    TWeakObjectPtr<const UFEBuildPieceDefinition> SupplyRowsDefinition;
    bool bSupplyRowsLocked = false;

    /** 보관함 패널(또는 탄약 패널)이 보고 있는 보관함 */
    UPROPERTY(Transient)
    TObjectPtr<AFEBuildStorage> StorageTarget;

    /** 지금 열린 보관함 패널이 발리스타 탄약 위젯이다 (닫을 때 같은 위젯을 닫고, 소지품은 볼트만 보인다) */
    bool bIsAmmoPanel = false;

    /** [Client Only] 서버가 보내 준 소지품 스냅샷. 패널 표시에만 쓴다 (판정은 전부 서버) */
    TArray<FFEBuildItemCost> CarriedItems;

    FTimerHandle NoticeTimer;

    /** [Client Only] 조준 HUD 갱신 타이머 (0.1초, 로컬 플레이어만 실제로 일함) */
    FTimerHandle AimTimer;

    int32 LastHudAmmo = -1;
    bool bLastHudReloading = false;

    /** 마지막으로 HUD 를 그린 발리스타. 내렸을 때 그 발리스타의 탄약 패널만 닫는다 */
    TWeakObjectPtr<AFEBuildBallista> HudBallista;
};
