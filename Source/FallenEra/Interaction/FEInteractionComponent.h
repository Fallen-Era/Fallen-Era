// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FEInteractionComponent.generated.h"

class UFEInteractPromptViewModel;

/**
 * 플레이어 캐릭터에 부착. E 입력(GA_Interact -> TryInteract)으로 대상을 고르고 서버에 요청한다.
 * 타게팅: 조준한 대상이 우선, 없으면 반경 안 후보 중 정면·근접 점수가 가장 좋은 것.
 * 건설 코드에 의존하지 않는다 (공용 승격 대비).
 */
UCLASS(ClassGroup = (FallenEra), meta = (BlueprintSpawnableComponent))
class FALLENERA_API UFEInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFEInteractionComponent();

    /** [Client Only] E. 대상을 찾아 서버에 상호작용 요청 */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Interaction")
    void TryInteract();

    /** [Client Only] 지금 상호작용하게 될 대상. UI 프롬프트가 폴링해서 쓴다. 없으면 nullptr */
    UFUNCTION(BlueprintCallable, Category = "FallenEra|Interaction")
    AActor* FindInteractTarget() const;

    /** Target 이 IFEInteractable 을 구현하고 지금 상호작용 가능한가 */
    static bool IsInteractable(const AActor* Target, AActor* InstigatorActor);
    
    /** HUD 위젯이 바인딩할 프롬프트 뷰모델. 지연 생성 */
    UFUNCTION(BlueprintPure, Category = "FallenEra|Interaction")
    UFEInteractPromptViewModel* GetPromptViewModel();

    virtual void BeginPlay() override;

protected:
    /** 조준 트레이스 거리 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Interaction", meta = (ClampMin = 50))
    float InteractDistance = 300.f;

    /** 조준한 것이 없을 때 주변 후보를 찾는 반경 (cm) */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Interaction", meta = (ClampMin = 0))
    float SearchRadius = 250.f;

    /** 주변 후보로 인정하는 정면 각도. 0.5 = 좌우 60도 */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Interaction", meta = (ClampMin = -1, ClampMax = 1))
    float MinFacingDot = 0.5f;

    /** 후보로 볼 오브젝트 타입. 기본: WorldStatic, WorldDynamic, Pawn, PhysicsBody, BuildPiece(GameTraceChannel2) */
    UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Interaction")
    TArray<TEnumAsByte<ECollisionChannel>> ObjectTypes;

    /** [Server RPC] 거리와 CanInteract 재검증 후 Interact 실행 */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerInteract(AActor* Target);

    /** [Client RPC] 서버 Interact 성공을 요청 클라이언트에 알려 InteractLocal 실행 */
    UFUNCTION(Client, Reliable)
    void ClientInteracted(AActor* Target);

private:
    bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
    FCollisionObjectQueryParams MakeObjectQuery() const;
    
    /** 0.1초마다. 로컬 플레이어일 때만 대상 탐색 → 프롬프트 갱신 */
    void UpdatePrompt();

    UPROPERTY(Transient)
    TObjectPtr<UFEInteractPromptViewModel> PromptViewModel;

    FTimerHandle PromptTimer;
};
