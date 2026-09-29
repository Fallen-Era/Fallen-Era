#include "HT/AI/AISettings.h"

FSAIAttackMontageSettings::FSAIAttackMontageSettings()
{
	TraceStartEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.TraceStart")), false);
	TraceEndEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.TraceEnd")), false);
	AttackResetEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.Reset")), false);
}
