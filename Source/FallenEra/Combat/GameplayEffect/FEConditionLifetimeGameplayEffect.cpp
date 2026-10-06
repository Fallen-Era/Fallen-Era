#include "Combat/GameplayEffect/FEConditionLifetimeGameplayEffect.h"

UFE_ConditionLifetimeGameplayEffect::UFE_ConditionLifetimeGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	bExecutePeriodicEffectOnApplication = false;
}
