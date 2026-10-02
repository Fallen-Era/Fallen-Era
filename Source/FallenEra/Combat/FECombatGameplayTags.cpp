#include "Combat/FECombatGameplayTags.h"

namespace FallenEraCombatGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_LeftClick, "Ability.Input.Combat.LeftClick");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_RightClick, "Ability.Input.Combat.RightClick");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_Swap, "Ability.Input.Combat.Swap");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Attack, "Ability.Combat.Attack");

	UE_DEFINE_GAMEPLAY_TAG(State_Condition_Bleeding, "State.Condition.Bleeding");
	UE_DEFINE_GAMEPLAY_TAG(State_Condition_Infection, "State.Condition.Infection");
	UE_DEFINE_GAMEPLAY_TAG(State_Immune_Knockback, "State.Immune.Knockback");

	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact, "GameplayCue.Impact");

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_AttackPower, "SetByCaller.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Condition_HealthLoss, "SetByCaller.Condition.HealthLoss");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_DefensePower, "SetByCaller.DefensePower");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_KnockbackResistance, "SetByCaller.KnockbackResistance");
}
