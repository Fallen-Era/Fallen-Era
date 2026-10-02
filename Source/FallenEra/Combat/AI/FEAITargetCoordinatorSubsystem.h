#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FEAITargetCoordinatorSubsystem.generated.h"

class AAIController;

/**
 * Server-side assignment registry used to spread enemies across valid players.
 * It has no tick; controllers update it only when their selected target changes.
 */
UCLASS()
class FALLENERA_API UFE_AITargetCoordinatorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void SetAssignedTarget(AAIController* Controller, AActor* TargetActor);
	void ClearAssignedTarget(AAIController* Controller);
	int32 GetTargetLoad(const AActor* TargetActor) const;

	virtual void Deinitialize() override;

private:
	void IncrementTarget(AActor* TargetActor);
	void DecrementTarget(AActor* TargetActor);

	TMap<TWeakObjectPtr<AAIController>, TWeakObjectPtr<AActor>> ControllerAssignments;
	TMap<TWeakObjectPtr<AActor>, int32> TargetLoads;
};
