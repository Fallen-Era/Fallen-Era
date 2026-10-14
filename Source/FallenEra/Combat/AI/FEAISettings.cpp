#include "Combat/AI/FEAISettings.h"

#include "Combat/FECombatGameplayTags.h"
#include "Combat/Ability/FEEnemyMeleeAttackAbility.h"

FSAIAttackMontageSettings::FSAIAttackMontageSettings()
{
	TraceStartEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.TraceStart")), false);
	TraceEndEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.TraceEnd")), false);
	AttackResetEventTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayEvent.Attack.Reset")), false);
}

FSAISettings::FSAISettings()
{
	AttackAbilityClass = UFE_EnemyMeleeAttackAbility::StaticClass();
	FFE_ConditionApplicationChance BleedingChance;
	BleedingChance.ConditionTag = FallenEraCombatGameplayTags::State_Condition_Bleeding;
	BleedingChance.Chance = 0.25f;
	ConditionApplicationChances.Add(BleedingChance);

	FFE_ConditionApplicationChance InfectionChance;
	InfectionChance.ConditionTag = FallenEraCombatGameplayTags::State_Condition_Infection;
	InfectionChance.Chance = 0.1f;
	ConditionApplicationChances.Add(InfectionChance);
}

void FSAISettings::GatherAssetPaths(TArray<FSoftObjectPath>& OutPaths, bool bIncludeServerAssets) const
{
	const auto AddPath = [&OutPaths](const FSoftObjectPath& Path)
	{
		if (!Path.IsNull()) { OutPaths.AddUnique(Path); }
	};
	if (bIncludeServerAssets) { AddPath(DamageEffect.ToSoftObjectPath()); }
	AddPath(DeathMontage.ToSoftObjectPath());
	AddPath(HitZoneMappingData.ToSoftObjectPath());
	AddPath(HitZoneMultiplierData.ToSoftObjectPath());
	if (!bIncludeServerAssets) { return; }
	AddPath(AttackAbilityClass.ToSoftObjectPath());
	for (const auto& AbilitySet : DefaultAbilitySets) { AddPath(AbilitySet.ToSoftObjectPath()); }
	for (const auto& Effect : StartupEffects) { AddPath(Effect.GameplayEffect.ToSoftObjectPath()); }
	for (const auto& Armor : DefaultArmorDataAssets) { AddPath(Armor.ToSoftObjectPath()); }
}

bool FSAISettings::AreAssetsLoaded(bool bIncludeServerAssets) const
{
	TArray<FSoftObjectPath> Paths;
	GatherAssetPaths(Paths, bIncludeServerAssets);
	for (const FSoftObjectPath& Path : Paths)
	{
		if (!Path.ResolveObject()) { return false; }
	}
	return true;
}
