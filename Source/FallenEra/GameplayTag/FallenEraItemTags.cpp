#include "FallenEraItemTags.h"

namespace FallenEraItemTags
{
	// ====================================================================
	// 1. 역할 범주 (Category)
	// ====================================================================
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Weapon, "Item.Category.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Weapon_Melee, "Item.Category.Weapon.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Weapon_Melee_Sword, "Item.Category.Weapon.Melee.Sword");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Weapon_Melee_Axe, "Item.Category.Weapon.Melee.Axe");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Weapon_Melee_Spear, "Item.Category.Weapon.Melee.Spear");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Weapon_Ranged, "Item.Category.Weapon.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Weapon_Ranged_Bow, "Item.Category.Weapon.Ranged.Bow");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Weapon_Ranged_Gun, "Item.Category.Weapon.Ranged.Gun");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Armor, "Item.Category.Armor");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Consumable, "Item.Category.Consumable");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Resource, "Item.Category.Resource");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Material, "Item.Category.Material");
	UE_DEFINE_GAMEPLAY_TAG(Item_Category_Building, "Item.Category.Building");

	// ====================================================================
	// 2. 재질 (Material)
	// ====================================================================
	UE_DEFINE_GAMEPLAY_TAG(Material_Wood, "Material.Wood");
	UE_DEFINE_GAMEPLAY_TAG(Material_Stone, "Material.Stone");
	UE_DEFINE_GAMEPLAY_TAG(Material_Metal, "Material.Metal");
	UE_DEFINE_GAMEPLAY_TAG(Material_Plant, "Material.Plant");
	UE_DEFINE_GAMEPLAY_TAG(Material_Flesh, "Material.Flesh");

	// ====================================================================
	// 3. 행동 및 특성 (Trait / Action)
	// ====================================================================
	UE_DEFINE_GAMEPLAY_TAG(Trait_Flammable, "Trait.Flammable");
	UE_DEFINE_GAMEPLAY_TAG(Trait_Smeltable, "Trait.Smeltable");
	UE_DEFINE_GAMEPLAY_TAG(Trait_Cookable, "Trait.Cookable");

	UE_DEFINE_GAMEPLAY_TAG(Action_Requires_Axe, "Action.Requires.Axe");
	UE_DEFINE_GAMEPLAY_TAG(Action_Requires_Pickaxe, "Action.Requires.Pickaxe");
	UE_DEFINE_GAMEPLAY_TAG(Action_Requires_Sickle, "Action.Requires.Sickle");

	// ====================================================================
	// 4. 아이템 스탯 (Stat)
	// ====================================================================
	UE_DEFINE_GAMEPLAY_TAG(Stat_Offense_PhysicalAttack, "Stat.Offense.PhysicalAttack");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Offense_AttackSpeed, "Stat.Offense.AttackSpeed");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Offense_CritChance, "Stat.Offense.CritChance");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Offense_CritDamage, "Stat.Offense.CritDamage");

	UE_DEFINE_GAMEPLAY_TAG(Stat_Defense_PhysicalArmor, "Stat.Defense.PhysicalArmor");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Defense_Evasion, "Stat.Defense.Evasion");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Defense_DamageReduction, "Stat.Defense.DamageReduction");

	UE_DEFINE_GAMEPLAY_TAG(Stat_Aux_Accuracy, "Stat.Aux.Accuracy");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Aux_Range, "Stat.Aux.Range");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Aux_Penetration, "Stat.Aux.Penetration");

	UE_DEFINE_GAMEPLAY_TAG(Stat_Special_Chance_Stun, "Stat.Special.Chance.Stun");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Special_Chance_Bleed, "Stat.Special.Chance.Bleed");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Special_Chance_Poison, "Stat.Special.Chance.Poison");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Special_DamageOverTime, "Stat.Special.DamageOverTime");

	UE_DEFINE_GAMEPLAY_TAG(Stat_Durability_Max, "Stat.Durability.Max");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Durability_DecreaseRate, "Stat.Durability.DecreaseRate");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weight_Value, "Stat.Weight.Value");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Weight_CapacityBonus, "Stat.Weight.CapacityBonus");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Repair_CostMultiplier, "Stat.Repair.CostMultiplier");
}
