#include "Combat/Component/FECombatComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/FECombatGameplayTags.h"

void UFE_CombatComponent::SetAttackStaggerProtection(UObject* Source, EFE_StaggerStrength Strength)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Source)) { return; }
	for (auto It = AttackStaggerProtections.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
	if (Strength == EFE_StaggerStrength::None) { AttackStaggerProtections.Remove(Source); }
	else { AttackStaggerProtections.Add(Source, Strength); }
}

void UFE_CombatComponent::ClearAttackStaggerProtection(UObject* Source)
{
	if (GetOwner() && GetOwner()->HasAuthority()) { AttackStaggerProtections.Remove(Source); }
}

EFE_StaggerStrength UFE_CombatComponent::GetIgnoredStaggerStrength() const
{
	EFE_StaggerStrength Strength = EFE_StaggerStrength::None;
	for (const auto& Entry : AttackStaggerProtections)
	{
		if (Entry.Key.IsValid() && Entry.Value > Strength) { Strength = Entry.Value; }
	}
	return Strength;
}

bool UFE_CombatComponent::CanActorPlayPhysicalHitReaction(AActor* Actor)
{
	if (!IsActorAlive(Actor)) { return false; }
	const UAbilitySystemComponent* ASC = FindAbilitySystemComponent(Actor);
	return !ASC || (!ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Immune_PhysicalReaction) &&
		!ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown));
}

FFE_DamageReactionDecision UFE_CombatComponent::ResolveDamageReaction(const FFE_AttackReactionData& ReactionData) const
{
	FFE_DamageReactionDecision Decision;
	const UAbilitySystemComponent* ASC = FindAbilitySystemComponent(GetOwner());
	if (bDeathCollisionDisabled || !IsActorAlive(GetOwner()) ||
		ReactionData.StaggerStrength <= GetIgnoredStaggerStrength() ||
		(ASC && ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Immune_Stun)))
	{
		return Decision;
	}
	Decision.bInterruptAttack = true;
	Decision.bPlayHitReaction = !ASC || !ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Immune_HitReaction);
	Decision.bApplyStun = ASC && FMath::IsFinite(ReactionData.StunDuration) && ReactionData.StunDuration > 0.0f;
	return Decision;
}
