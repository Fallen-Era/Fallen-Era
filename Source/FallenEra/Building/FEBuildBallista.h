// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildDefense.h"
#include "FEBuildBallista.generated.h"

class UCameraComponent;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UAbilitySystemComponent;
class APlayerController;
class AFE_CombatProjectile;
class ACharacter;

/**
 * 수동 발리스타. 완성 상태에서 E 로 탑승(한 번에 1명) → 이동 잠금·발리스타 카메라·조준(좌우 ±MaxYaw, 상하 MinPitch~MaxPitch).
 * 탄약 칸 1개(볼트만 — 보관함 서버 로직 재사용). 전용 탄약 UI 는 탑승 중 Tab, 또는 내린 상태에서 발리스타를 조준하고 Tab.
 * 탑승은 발리스타가 전부 처리한다. 서버가 조작자를 Owner 로 지정하므로 그 클라가 발리스타에 직접 Server RPC(발사·조준·내리기)를 보내고,
 * 조작자 기계에서는 발리스타가 시야·입력·이동 잠금·조준을 맡는다. 플레이어 쪽 컴포넌트는 발리스타를 모른다 (탑승 GA 는 FindOperatedBy 로 찾는다).
 * 머리(Head)는 보이기만 하는 부품이라 콜리전이 없다 — 총신이 벽·난간 자리 위로 나가도 그 설치를 막지 않는다.
 */
UCLASS()
class FALLENERA_API AFEBuildBallista : public AFEBuildDefense
{
    GENERATED_BODY()

public:
	AFEBuildBallista();

	/** OperatorActor 가 지금 타고 있는 발리스타. 없으면 nullptr. 리플리케이트된 Operator 로 찾으므로 어느 기계에서나 같다.
	 *  탑승 GA(발사·내리기)와 탑승 HUD 가 "내 발리스타"를 찾을 때 쓴다 */
	UFUNCTION(BlueprintPure, Category = "FallenEra|Building|Ballista")
	static AFEBuildBallista* FindOperatedBy(const AActor* OperatorActor);

	/** [Client Only] 발사 요청. GA_Build_BallistaFire(좌클릭)가 부른다. 장전 중이면 무시, 탄약이 없으면 안내. 이 기계의 플레이어가 탄 발리스타에서만 동작 */
	UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|Ballista")
	void RequestFire();

	/** [Client Only] 내리기 요청. GA_Build_BallistaExit(E·우클릭)가 부른다 */
	UFUNCTION(BlueprintCallable, Category = "FallenEra|Building|Ballista")
	void RequestExit();

	/** 탄약 칸의 볼트 수. Contents 가 리플리케이트되므로 클라 HUD 에서도 맞다 */
	int32 GetAmmoCount() const;

	/** 장전 중인가. 서버 시각과 리플리케이트된 NextFireTime 으로 판단하므로 누가 쐈든 모든 기계에서 같다 (탑승 HUD·발사 입력) */
	bool IsReloading() const;

	virtual bool AcceptsItem(FGameplayTag ItemTag, FText& OutReason) const override;
	virtual FText GetSlotLabel(int32 SlotIndex) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 이 기계의 플레이어가 탔을 때만 켜진다 (조준 추종) */
	virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual bool CanInteractBuilt(AActor* InstigatorActor) const override;
    virtual FText GetInteractTextBuilt(AActor* InstigatorActor) const override;
    virtual void InteractBuilt(AActor* InstigatorActor) override;
    virtual void InteractBuiltLocal(AActor* InstigatorActor) override;

    UPROPERTY(ReplicatedUsing = OnRep_Operator)
    TObjectPtr<APawn> Operator;

    /** 조준 상대 각도(도). 조작자 기계는 로컬 값을 쓰고, 다른 기계는 이 값으로 머리를 돌린다 */
    UPROPERTY(ReplicatedUsing = OnRep_Aim)
    int8 AimYaw = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Aim)
    int8 AimPitch = 0;
    
    /** 다음 발사를 받을 수 있는 서버 시각. 서버가 FEBuilding::ConsumeRequestSlot 으로 ReloadTime 씩 누적하고, 모든 기계가 IsReloading 으로 읽는다.
     *  발리스타마다 따로 센다 — 조작자가 바뀌거나 다른 발리스타로 옮겨 타도 각자 값이 맞다 */
    UPROPERTY(Replicated)
    double NextFireTime = 0.0;

    /** 탄약 칸에 들어가는 아이템 (임시 태그 Item.Ammo.Bolt) */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (Categories = "Item"))
    FGameplayTag AmmoTag;

	/** 좌우 조준 한계 (±도). 다른 화면용 조준 값이 int8(±127)이라 127 까지 */
	UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = 0, ClampMax = 127))
	float MaxYaw = 60.f;

    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = -89, ClampMax = 0))
    float MinPitch = -10.f;

    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = 0, ClampMax = 89))
    float MaxPitch = 30.f;

    /** 조작자가 이 거리보다 멀어지면(밀림·텔레포트) 자동으로 내린다 */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = 0))
    float MaxOperatorDistance = 300.f;

    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista")
    TSoftObjectPtr<UInputMappingContext> MappingContext;

    /** 전투 IMC(20)·빌드 모드 IMC(30)보다 높아야 좌·우클릭과 E 를 가져온다 */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista")
    int32 MappingPriority = 40;
    
    /** 쏘는 볼트 (AFE_CombatProjectile 자식 BP — BP_Ballista_Bolt).
     *  소프트 참조: BeginPlay 에서 모든 기계가 미리 받는다 (서버는 발사, 클라는 리플리케이트된 볼트를 띄울 때 필요) */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista")
    TSoftClassPtr<AFE_CombatProjectile> BoltClass;

    /** 발사 속도 (cm/s) */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = 0))
    float BoltSpeed = 6000.f;

    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = 0))
    float BoltGravityScale = 0.3f;

    /** 발사 후 다음 발사까지 (초). 발리스타마다 따로 센다 — 조작자가 내렸다 다시 타도 유지 */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = 0.1))
    float ReloadTime = 2.f;

    /** 볼트가 화면 중앙(카메라 정면)과 만나는 거리 (cm).
     *  카메라가 총신보다 위에 있어 총신 방향 그대로 쏘면 늘 화면 중앙보다 아래에 맞는다 → 머즐에서 이 거리의 카메라 정면 지점을 향해 쏜다 */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = 100))
    float AimConvergeDistance = 3000.f;

    /** 회전하는 머리(총신). 메시는 BP 에서 지정. 보이기만 한다(콜리전 없음) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Ballista")
    TObjectPtr<UStaticMeshComponent> Head;

    /** 탑승 중 시야. 머리에 붙어 같이 돈다 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Ballista")
    TObjectPtr<UCameraComponent> Camera;

    /** 탑승하면 조작자가 서는 자리 (받침 뒤쪽, 바닥 높이) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Ballista")
    TObjectPtr<USceneComponent> Seat;
    
    /** 볼트가 나오는 곳. Head 에 붙어 같이 돈다 (총신 끝에 둔다) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Ballista")
    TObjectPtr<USceneComponent> Muzzle;

private:
    /** [Server RPC] 발사. 조준은 정밀값(float, 발리스타 기준 상대 각도) — 리플리케이트용 int8 은 1° 단위라 멀리서 빗나간다 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerFire(float Yaw, float Pitch);

    /** [Server RPC] 조준(상대 각도, 도). 다른 플레이어에게 보이는 용도라 Unreliable, 바뀌면 10Hz 이하 + 그대로여도 0.5초마다 재전송 */
    UFUNCTION(Server, Unreliable, WithValidation)
    void ServerSetAim(int8 Yaw, int8 Pitch);

    /** [Server RPC] 내림 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerExit();

    /** [Client RPC] 서버만 아는 발사 실패 사유를 조작자 화면에. Owner = 조작자라 그 플레이어에게만 간다 */
    UFUNCTION(Client, Reliable)
    void ClientShowNotice(const FText& Text);

    UFUNCTION()
    void OnRep_Operator();

    UFUNCTION()
    void OnRep_Aim();

    /** [Server Only] 조작자 지정, nullptr = 내림. Owner·이동 잠금·좌석 이동을 함께 한다 */
    void SetOperator(APawn* NewOperator);

    /** [Server Only] 탄약·볼트 준비·장전을 확인하고 투사체 풀로 볼트를 쏜다. Yaw/Pitch = 발리스타 기준 상대 각도(도, 정밀값).
     *  실패하면 false — OutReason 이 비어 있지 않으면 플레이어에게 보여 줄 사유 */
    bool TryFire(APawn* InstigatorPawn, float Yaw, float Pitch, FText& OutReason);

    /** [Server Only] 조작자가 보낸 조준(상대 각도, 도)을 범위로 잘라 리플리케이트 */
    void SetReplicatedAim(int8 Yaw, int8 Pitch);

    /** [Server Only] 탑승 중 0.5초마다: 조작자가 사라졌거나 죽었거나 멀어졌으면 내린다 */
    void CheckOperator();

    /** 머리에 상대 각도 적용 */
    void SetHeadAim(float Yaw, float Pitch);

    /** 조작자 이동 잠금·해제 (서버 SetOperator 와 조작자 기계의 Begin/EndLocalOperation 이 같이 쓴다).
     *  잠금 = MOVE_Custom + 건설 전용 번호. 해제는 지금 모드가 그 잠금일 때만 걷기로 되돌린다 —
     *  탑승 중 사망하면 HT 사망 처리가 MOVE_None 으로 바꾸므로 그 잠금은 건드리지 않는다 */
    static void LockOperatorMovement(ACharacter* Character);
    static void UnlockOperatorMovement(ACharacter* Character);

    /** [Client Only] 이 기계의 플레이어가 탔다: 이동 잠금·시야·입력·State.Building.Mounted·조준 틱 */
    void BeginLocalOperation(APawn* Pawn);

    /** [Client Only] 내렸다 (그 폰이 이미 사라졌어도). Begin 에서 기억해 둔 컨트롤러·입력·ASC 에서 되돌린다. 탑승 중이 아니면 아무것도 안 한다 */
    void EndLocalOperation();

    /** [Client Only] 탑승 IMC 를 넣는다. 이미 넣었거나 IMC 가 로드 전이면 아무것도 안 한다 — 탑승 때 한 번, 로드가 늦으면 로드 완료 때 한 번 더 */
    void AddLocalInput();

    /** [Client Only] 매 프레임: 컨트롤 회전 → 머리. 바뀌었으면 10Hz 이하로, 그대로여도 0.5초마다 서버에 보낸다 */
    void UpdateLocalAim();

    /** [Client Only] 컨트롤 회전을 조준 범위로 잘라 머리에 바로 적용. 잘린 상대 각도(X = Yaw, Y = Pitch, 도)를 돌려준다 */
    FVector2D ApplyLocalAim(const FRotator& ControlRotation);

    /** [Client Only] 탑승 IMC 비동기 로드 완료. 로드가 끝나기 전에 이 기계의 플레이어가 탔으면 이제 입력을 넣는다 */
    void HandleMappingContextLoaded();

    /** [Client Only] 이 기계의 플레이어가 타고 있다 (BeginLocalOperation ~ EndLocalOperation) */
    bool bLocalOperation = false;

    /** [Client Only] Begin 에서 기억해 둔 것들. 내릴 때 폰이 이미 사라졌을 수 있어 여기서 되돌린다
     *  (컨트롤러·로컬 플레이어 입력·PlayerState 의 ASC 는 폰보다 오래 산다) */
    TWeakObjectPtr<APawn> LocalOperatorPawn;
    TWeakObjectPtr<APlayerController> LocalController;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> LocalInputOwner;
    TWeakObjectPtr<UAbilitySystemComponent> LocalTagOwner;

    int8 LastSentAimYaw = 0;
    int8 LastSentAimPitch = 0;
    double LastAimSendTime = -1.0;

    /** [Server Only] 요청 간격 제한 (FEBuilding::ConsumeRequestSlot). 장전(ReloadTime)과 별개로,
     *  조작된 클라의 빈 발사·조준 요청이 서버 처리와 Reliable 안내를 무제한으로 만들지 않게 */
    double NextFireRequestTime = 0.0;
    double NextAimRequestTime = 0.0;

    FTimerHandle OperatorCheckTimer;
    TSharedPtr<FStreamableHandle> MappingLoadHandle;
    TSharedPtr<FStreamableHandle> BoltLoadHandle;
};