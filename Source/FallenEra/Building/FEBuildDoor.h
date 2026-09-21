// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildPiece.h"
#include "FEBuildDoor.generated.h"

class UStaticMesh;

/**
 * 문틀(Mesh) + 여닫히는 문짝(Panel). 문틀 메시는 정의(DA)에서, 문짝 메시와 경첩 위치는 BP 자식(BP_Piece_Door)에서 지정.
 * bIsOpen 은 서버가 소유하고 리플리케이트. 회전은 즉시 (부드러운 회전은 S11).
 */
UCLASS()
class FALLENERA_API AFEBuildDoor : public AFEBuildPiece
{
	GENERATED_BODY()

public:
	AFEBuildDoor();

	UFUNCTION(BlueprintPure, Category = "FallenEra|Building|Door")
	bool IsOpen() const;

	virtual void SetPreviewValid(bool bIsValid) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_IsOpen)
	bool bIsOpen = false;

	/** 문짝 메시. BP 자식에서 지정 (예: SM_Proto_DoorPanel, 경첩 모서리 피벗) */
	UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Door")
	TSoftObjectPtr<UStaticMesh> PanelMesh;

	/** 문틀 로컬 기준 경첩 위치 */
	UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Door")
	FVector HingeOffset = FVector(-60.f, 0.f, 0.f);

	/** 열렸을 때 문짝 Yaw */
	UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Building|Door")
	float OpenYaw = 90.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Door")
	TObjectPtr<UStaticMeshComponent> Panel;

	UFUNCTION()
	void OnRep_IsOpen();

	virtual bool CanInteractBuilt(AActor* InstigatorActor) const override;
	virtual FText GetInteractTextBuilt(AActor* InstigatorActor) const override;
	virtual void InteractBuilt(AActor* InstigatorActor) override;
	virtual void OnApplyState(EFEBuildPieceState NewState) override;
};
