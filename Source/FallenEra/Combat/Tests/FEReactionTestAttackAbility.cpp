#include "Combat/Tests/FEReactionTestAttackAbility.h"

#include "Combat/Ability/Tasks/FEMeleeTraceTask.h"
#include "Combat/Weapon/FEWeaponItemData.h"

UFE_ReactionTestAttackAbility::UFE_ReactionTestAttackAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UFE_ReactionTestAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UFE_WeaponItemData* Weapon = Cast<UFE_WeaponItemData>(GetCurrentSourceObject());
	CacheAttackContext(Weapon, Weapon && !Weapon->Template.AttackActions.IsEmpty() ? Weapon->Template.AttackActions[0].Get() : nullptr);
	TraceTask = UFE_MeleeTraceTask::StartMeleeTrace(this);
	TraceTask->ReadyForActivation();
}

bool UFE_ReactionTestAttackAbility::IsTraceActive() const
{
	return TraceTask && TraceTask->IsActive();
}
