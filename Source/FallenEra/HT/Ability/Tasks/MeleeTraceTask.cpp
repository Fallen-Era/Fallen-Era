#include "HT/Ability/Tasks/MeleeTraceTask.h"

UFE_MeleeTraceTask::UFE_MeleeTraceTask()
{
	bTickingTask = true;
}

UFE_MeleeTraceTask* UFE_MeleeTraceTask::StartMeleeTrace(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<UFE_MeleeTraceTask>(OwningAbility);
}

void UFE_MeleeTraceTask::Activate()
{
	SetWaitingOnAvatar();
}

void UFE_MeleeTraceTask::TickTask(float DeltaTime)
{
	(void)DeltaTime;

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnTraceTick.Broadcast();
	}
}
