#include "Combat/FECombatGameplayTags.h"

namespace FallenEraCombatGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_AmmoMultiplier, "SetByCaller.Damage.AmmoMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_SelectingAmmo, "State.Combat.SelectingAmmo");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_LeftClick, "Ability.Input.Combat.LeftClick");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_RightClick, "Ability.Input.Combat.RightClick");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_Swap, "Ability.Input.Combat.Swap");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Combat_Reload, "Ability.Input.Combat.Reload");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Reload, "Ability.Combat.Reload");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Reloading, "State.Combat.Reloading");
	UE_DEFINE_GAMEPLAY_TAG(Item_Ammo_Rifle, "Item.Ammo.Rifle");
	UE_DEFINE_GAMEPLAY_TAG(Item_Ammo_Arrow, "Item.Ammo.Arrow");
	UE_DEFINE_GAMEPLAY_TAG(Item_Ammo_Grenade, "Item.Ammo.Grenade");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Attack, "Ability.Combat.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Aim, "Ability.Combat.Aim");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Aiming, "State.Combat.Aiming");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_KnockedDown, "State.Combat.KnockedDown");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_WeaponSlot_Sight, "Equipment.WeaponSlot.Sight");

	UE_DEFINE_GAMEPLAY_TAG(State_Condition_Bleeding, "State.Condition.Bleeding");
	UE_DEFINE_GAMEPLAY_TAG(State_Condition_Infection, "State.Condition.Infection");
	UE_DEFINE_GAMEPLAY_TAG(State_Immune_Knockback, "State.Immune.Knockback");
	UE_DEFINE_GAMEPLAY_TAG(State_Immune_Stun, "State.Immune.Stun");
	UE_DEFINE_GAMEPLAY_TAG(State_Immune_HitReaction, "State.Immune.HitReaction");
	UE_DEFINE_GAMEPLAY_TAG(State_Immune_PhysicalReaction, "State.Immune.PhysicalReaction");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Walking, "State.Movement.Walking");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Sprinting, "State.Movement.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_Stamina_Exhausted, "State.Stamina.Exhausted");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ActivateFail_Stamina, "Ability.ActivateFail.Stamina");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Stamina_Change, "SetByCaller.Stamina.Change");

	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact, "GameplayCue.Impact");

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_AttackPower, "SetByCaller.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_HitRegionMultiplier, "SetByCaller.Damage.HitRegionMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_HitQualityMultiplier, "SetByCaller.Damage.HitQualityMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Condition_HealthLoss, "SetByCaller.Condition.HealthLoss");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_DefensePower, "SetByCaller.DefensePower");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_KnockbackResistance, "SetByCaller.KnockbackResistance");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Accuracy, "SetByCaller.Accuracy");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_RecoilControl, "SetByCaller.RecoilControl");
}
