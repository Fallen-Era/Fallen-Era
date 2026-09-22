#include "AbilitySystem/FallenEraGameplayTags.h"

namespace FallenEraGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input, "Ability.Input");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Jump, "Ability.Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Primary, "Ability.Input.Primary");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Secondary, "Ability.Input.Secondary");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_LeftClick, "Ability.Input.Combat.LeftClick");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_RightClick, "Ability.Input.Combat.RightClick");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_Swap, "Ability.Input.Combat.Swap");

	UE_DEFINE_GAMEPLAY_TAG(State_AbilitySystem_Initialized, "State.AbilitySystem.Initialized");
	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Stunned, "State.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(State_InputBlocked, "State.InputBlocked");
	UE_DEFINE_GAMEPLAY_TAG(State_MovementBlocked, "State.MovementBlocked");

	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Damage, "GameplayCue.Damage");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Heal, "GameplayCue.Heal");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact, "GameplayCue.Impact");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_AttackPower, "SetByCaller.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Heal, "SetByCaller.Heal");
}
