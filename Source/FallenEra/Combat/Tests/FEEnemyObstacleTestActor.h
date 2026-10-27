#pragma once

#include "Combat/AI/FEEnemyAIController.h"
#include "Combat/Interface/FEDamageable.h"
#include "GenericTeamAgentInterface.h"
#include "FEEnemyObstacleTestActor.generated.h"

class UBoxComponent;

/** Construction-independent receiver with a deliberately offset collision surface. */
UCLASS(NotBlueprintable, Transient)
class AFE_EnemyObstacleTestActor : public AActor, public IFE_Damageable, public IGenericTeamAgentInterface
{
	GENERATED_BODY()
public:
	AFE_EnemyObstacleTestActor();
	virtual FGenericTeamId GetGenericTeamId() const override { return Team; }
	virtual FFE_CombatDamageResult ReceiveCombatDamage_Implementation(const FFE_CombatDamageRequest& Request) override;
	UPROPERTY() TObjectPtr<UBoxComponent> Box;
	FGenericTeamId Team = FGenericTeamId(1);
	int32 HitCount = 0;
	FFE_CombatDamageRequest LastRequest;
};

/** Scripted paths isolate decision/partial-completion behavior from NavMesh generation in automation. */
UCLASS(NotBlueprintable, Transient)
class AFE_EnemyObstacleTestController : public AFE_EnemyAIController
{
	GENERATED_BODY()
public:
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr) override;
	bool bProvidePartialPath = false;
	FVector PartialPathDirection = FVector::ForwardVector;
	int32 MoveRequests = 0;
};
