#include "Combat/AI/AITargetCoordinatorSubsystem.h"

#include "AIController.h"

void UFE_AITargetCoordinatorSubsystem::SetAssignedTarget(
	AAIController* Controller,
	AActor* TargetActor)
{
	if (!IsValid(Controller))
	{
		return;
	}

	const TWeakObjectPtr<AAIController> ControllerKey(Controller);
	if (const TWeakObjectPtr<AActor>* ExistingTarget = ControllerAssignments.Find(ControllerKey))
	{
		if (ExistingTarget->Get() == TargetActor)
		{
			return;
		}
		DecrementTarget(ExistingTarget->Get());
	}

	if (IsValid(TargetActor))
	{
		ControllerAssignments.FindOrAdd(ControllerKey) = TargetActor;
		IncrementTarget(TargetActor);
	}
	else
	{
		ControllerAssignments.Remove(ControllerKey);
	}
}

void UFE_AITargetCoordinatorSubsystem::ClearAssignedTarget(AAIController* Controller)
{
	SetAssignedTarget(Controller, nullptr);
}

int32 UFE_AITargetCoordinatorSubsystem::GetTargetLoad(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return 0;
	}
	const TWeakObjectPtr<AActor> TargetKey(const_cast<AActor*>(TargetActor));
	return TargetLoads.FindRef(TargetKey);
}

void UFE_AITargetCoordinatorSubsystem::Deinitialize()
{
	ControllerAssignments.Reset();
	TargetLoads.Reset();
	Super::Deinitialize();
}

void UFE_AITargetCoordinatorSubsystem::IncrementTarget(AActor* TargetActor)
{
	if (IsValid(TargetActor))
	{
		++TargetLoads.FindOrAdd(TargetActor);
	}
}

void UFE_AITargetCoordinatorSubsystem::DecrementTarget(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	const TWeakObjectPtr<AActor> TargetKey(TargetActor);
	if (int32* Load = TargetLoads.Find(TargetKey))
	{
		--(*Load);
		if (*Load <= 0)
		{
			TargetLoads.Remove(TargetKey);
		}
	}
}
