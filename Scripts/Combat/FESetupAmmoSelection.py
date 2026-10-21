"""Editor-only initial setup. Run with the editor closed using PythonScript commandlet.

Updates only the listed Combat weapons/player BP and creates dedicated ammo DAs.
Back up those existing packages before running. No external marketplace assets required.
"""
import unreal


def tag(name):
    result = unreal.GameplayTag()
    if not result.import_text('(TagName="' + name + '")'):
        raise RuntimeError("Invalid tag: " + name)
    return result


tools = unreal.AssetToolsHelpers.get_asset_tools()
directory = "/Game/Combat/Equiment/Ammo"
definitions = {}
for caliber in ("9mm", "5mm", "7mm"):
    for variant, multiplier, label in (("Standard", 1.0, "일반탄"), ("Enhanced", 1.2, "강화탄")):
        name = "DA_Ammo_" + caliber + "_" + variant
        asset = unreal.load_asset(directory + "/" + name)
        if not asset:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.FE_AmmoItemData)
            asset = tools.create_asset(name, directory, unreal.FE_AmmoItemData, factory)
        template = asset.get_editor_property("template")
        template.set_editor_property("ammo_type", tag("Item.Ammo." + caliber))
        template.set_editor_property("item_tag", tag("Item.Ammo." + caliber + "." + variant))
        template.set_editor_property("display_name", unreal.Text(caliber + " " + label))
        asset.set_editor_property("template", template)
        instance = asset.get_editor_property("instance")
        stats = instance.get_editor_property("item_data")
        stats.set_editor_property("damage_multiplier", multiplier)
        instance.set_editor_property("item_data", stats)
        asset.set_editor_property("instance", instance)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        definitions[(caliber, variant)] = asset

weapons = (("/Game/Combat/Equiment/Pistol/DA_PistolData", "9mm"),
           ("/Game/Combat/Equiment/Rifle/DA_RifleData", "5mm"))
for path, caliber in weapons:
    weapon = unreal.load_asset(path)
    if not weapon:
        raise RuntimeError("Missing weapon " + path)
    template = weapon.get_editor_property("template")
    settings = template.get_editor_property("ammo_settings")
    settings.set_editor_property("ammo_type", tag("Item.Ammo." + caliber))
    template.set_editor_property("ammo_settings", settings)
    weapon.set_editor_property("template", template)
    instance = weapon.get_editor_property("instance")
    round_data = instance.get_editor_property("loaded_ammo_data")
    definition = definitions[(caliber, "Standard")]
    round_data.set_editor_property("ammo_type", tag("Item.Ammo." + caliber))
    round_data.set_editor_property("ammo_data", definition)
    round_data.set_editor_property("instance", definition.get_editor_property("instance"))
    instance.set_editor_property("loaded_ammo_data", round_data)
    weapon.set_editor_property("instance", instance)
    unreal.EditorAssetLibrary.save_loaded_asset(weapon, only_if_is_dirty=False)

blueprint = unreal.load_asset("/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter")
character = unreal.get_default_object(blueprint.generated_class())
equipment = character.get_component_by_class(unreal.FE_EquipmentComponent)
if not equipment:
    raise RuntimeError("Player EquipmentComponent was not found")
existing = list(equipment.get_editor_property("test_ammo_inventory"))
updated = []
for stack in existing:
    type_name = str(stack.get_editor_property("ammo_type").get_editor_property("tag_name"))
    if type_name not in ("Item.Ammo.Rifle", "Item.Ammo.9mm", "Item.Ammo.5mm", "Item.Ammo.7mm"):
        updated.append(stack)
for (caliber, variant), definition in definitions.items():
    stack = unreal.FE_AmmoStack()
    stack.set_editor_property("ammo_type", tag("Item.Ammo." + caliber))
    stack.set_editor_property("ammo_data", definition)
    stack.set_editor_property("count", 90 if variant == "Standard" else 30)
    stack.set_editor_property("use_definition_stats", True)
    updated.append(stack)
equipment.set_editor_property("test_ammo_inventory", updated)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
unreal.log("FE_AMMO_SETUP_COMPLETE: six definitions, pistol 9mm, rifle 5mm, player test inventory")
