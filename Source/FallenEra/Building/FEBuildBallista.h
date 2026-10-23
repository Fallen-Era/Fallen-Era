// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildDefense.h"
#include "FEBuildBallista.generated.h"

class UCameraComponent;
class UInputMappingContext;
class AFE_CombatProjectile;

/**
 * 수동 발리스타. 완성 상태에서 E 로 탑승(한 번에 1명) → 이동 잠금·발리스타 카메라·조준(좌우 ±MaxYaw, 상하 MinPitch~MaxPitch).
 * 탄약 칸 1개(볼트만 — 보관함 서버 로직 재사용). 전용 탄약 UI 는 탑승 중 Tab, 또는 내린 상태에서 발리스타를 조준하고 Tab.
 * 상태(Operator·조준)는 서버가 소유하고 리플리케이트한다. 클라 요청은 조작자의 UFEBuildingComponent RPC 로 들어온다
 * (발리스타는 플레이어 소유가 아니라 직접 Server RPC 를 받을 수 없다).
 * 머리(Head)는 보이기만 하는 부품이라 콜리전이 없다 — 총신이 벽·난간 자리 위로 나가도 그 설치를 막지 않는다.
 */
UCLASS()
class FALLENERA_API AFEBuildBallista : public AFEBuildDefense
{
    GENERATED_BODY()

public:
    AFEBuildBallista();

    /** [Server Only] 조작자 지정, nullptr = 내림. 이동 잠금·복구와 좌석 이동을 함께 한다 */
    void SetOperator(APawn* NewOperator);

    APawn* GetOperator() const;

    /** [Client Only] 조작자의 컨트롤 회전을 조준 범위로 잘라 머리에 바로 적용. 잘린 상대 각도(X = Yaw, Y = Pitch, 도)를 돌려준다 */
    FVector2D ApplyLocalAim(const FRotator& ControlRotation);

    /** [Server Only] 조작자가 보낸 조준(상대 각도, 도)을 범위로 잘라 리플리케이트 */
    void SetReplicatedAim(int8 Yaw, int8 Pitch);

    /** 탑승 중 추가하는 입력(좌클릭 발사·E/우클릭 내림). 시작 때 비동기로 미리 받아 둔다 */
    UInputMappingContext* GetMappingContext() const;

    int32 GetMappingPriority() const;
    
    /** [Server Only] 조작자의 발사 요청. Yaw/Pitch = 발리스타 기준 상대 각도(도, 정밀값).
     *  탄약·장전·볼트 준비를 확인하고 투사체 풀로 볼트를 쏜다. 실패하면 false — OutReason 이 비어 있지 않으면 플레이어에게 보여 줄 사유 */
    bool TryFire(APawn* InstigatorPawn, float Yaw, float Pitch, FText& OutReason);

    /** 탄약 칸의 볼트 수. Contents 가 리플리케이트되므로 클라 HUD 에서도 맞다 */
    int32 GetAmmoCount() const;

    float GetReloadTime() const;

    virtual bool AcceptsItem(FGameplayTag ItemTag, FText& OutReason) const override;
    virtual FText GetSlotLabel(int32 SlotIndex) const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

    /** 탄약 칸에 들어가는 아이템 (임시 태그 Item.Ammo.Bolt) */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (Categories = "Item"))
    FGameplayTag AmmoTag;

    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Ballista", meta = (ClampMin = 0, ClampMax = 180))
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
    UFUNCTION()
    void OnRep_Operator();

    UFUNCTION()
    void OnRep_Aim();

    /** [Server Only] 탑승 중 0.5초마다: 조작자가 사라졌거나 죽었거나 멀어졌으면 내린다 */
    void CheckOperator();

    /** 머리에 상대 각도 적용 */
    void SetHeadAim(float Yaw, float Pitch);

    /** 이 기계가 마지막으로 알던 조작자. 바뀌면 이전 조작자 기계에 "내림"을 알린다 */
    TWeakObjectPtr<APawn> LocalKnownOperator;

    FTimerHandle OperatorCheckTimer;
    TSharedPtr<FStreamableHandle> MappingLoadHandle;
    
    TSharedPtr<FStreamableHandle> BoltLoadHandle;

    /** [Server Only] 다음 발사를 받을 수 있는 시각. FEBuilding::ConsumeRequestSlot 으로 ReloadTime 씩 누적 */
    double NextFireTime = 0.0;
};