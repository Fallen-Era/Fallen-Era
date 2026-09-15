// Fallen Era 건설 시스템 (KJH)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/PrimaryAssetId.h"
#include "Building/FEBuildingTypes.h"
#include "FEBuildPiece.generated.h"

class UFEBuildPieceDefinition;
class UStaticMeshComponent;
struct FStreamableHandle;

/**
 * 배치된 구조물 1개. 같은 액터가 Preview -> Blueprint -> Built 로 상태만 바뀌므로
 * 소켓 링크가 상태 변화에도 유지된다. 리플리케이트되며 상태는 서버가 소유한다.
 * 클라이언트는 PieceId + State 만 받고, 정의 에셋에서 시각 요소를 스스로 복원한다.
 */
UCLASS()
class FALLENERA_API AFEBuildPiece : public AActor
{
    GENERATED_BODY()

public:
    AFEBuildPiece();

    /** [Server Only] (또는 로컬 프리뷰) SpawnActorDeferred 와 FinishSpawning 사이에 호출. 정의의 Runtime 번들이 로드되어 있어야 함. */
    void InitializePiece(const UFEBuildPieceDefinition* InDefinition, EFEBuildPieceState InState);

    /** [Server Only] 클라이언트는 OnRep_State 로 따라온다. */
    void SetState(EFEBuildPieceState NewState);

    /** [Client Only] 프리뷰 고스트 색상(유효/무효) 교체 */
    void SetPreviewValid(bool bIsValid);

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    const UFEBuildPieceDefinition* GetDefinition() const;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    EFEBuildPieceState GetState() const;

    UFUNCTION(BlueprintPure, Category = "FallenEra|Building")
    UStaticMeshComponent* GetMesh() const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_PieceId)
    FPrimaryAssetId PieceId;

    UPROPERTY(ReplicatedUsing = OnRep_State)
    EFEBuildPieceState State = EFEBuildPieceState::Blueprint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UFUNCTION()
    void OnRep_PieceId();

    UFUNCTION()
    void OnRep_State();

    /** BP 훅 (VFX/SFX 용). ApplyState 가 실행될 때마다 호출되며 최초 1회도 포함. */
    UFUNCTION(BlueprintImplementableEvent, Category = "FallenEra|Building")
    void OnStateChanged(EFEBuildPieceState NewState);

private:
    /** [Client Only] PieceId 를 로드된 정의로 해석한 뒤 ApplyState. */
    void RequestDefinition();
    void HandleDefinitionLoaded();

    /** 현재 상태에 맞는 메시/콜리전 프로파일/머티리얼 적용. 여러 번 호출해도 안전(멱등). */
    void ApplyState();

    /** PieceId 에서 해석됨(클라이언트) 또는 직접 전달됨(서버/프리뷰). 리플리케이트하지 않음. */
    UPROPERTY(Transient)
    TObjectPtr<const UFEBuildPieceDefinition> Definition;

    TSharedPtr<FStreamableHandle> DefinitionLoadHandle;
};
