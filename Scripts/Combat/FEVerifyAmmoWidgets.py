"""Read-only validation of saved WBP classes, brush assets, designer trees and player linkage."""
import unreal

radial = unreal.load_asset('/Game/Combat/UI/WBP/WBP_FE_AmmoRadial')
option = unreal.load_asset('/Game/Combat/UI/WBP/WBP_FE_AmmoOption')
for bp, names in ((radial, ('SizeBox_Wheel', 'CanvasPanel_Options', 'Text_Title', 'Text_Hint')),
                  (option, ('Image_Sector', 'Border_Content', 'Text_AmmoName', 'Text_Count', 'Text_Damage'))):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    tree = unreal.find_object(bp, 'WidgetTree')
    for name in names:
        assert unreal.find_object(tree, name), name
defaults = unreal.get_default_object(radial.generated_class())
assert str(defaults.get_editor_property('option_widget_class')) == str(option.generated_class()) or 'WBP_FE_AmmoOption_C' in str(defaults.get_editor_property('option_widget_class'))
assert not defaults.get_editor_property('is_focusable')
assert defaults.get_editor_property('visibility') == unreal.SlateVisibility.VISIBLE
sector = unreal.find_object(unreal.find_object(option, 'WidgetTree'), 'Image_Sector')
brush = sector.get_editor_property('brush')
assert brush.get_editor_property('draw_as') == unreal.SlateBrushDrawType.IMAGE
material = unreal.load_asset('/Game/Combat/UI/Materials/M_UI_AmmoSector')
assert brush.get_editor_property('resource_object') == material
assert material.get_editor_property('material_domain') == unreal.MaterialDomain.MD_UI
lib = unreal.MaterialEditingLibrary
for node in lib.get_material_expressions(material):
    unreal.log('FE_AMMO_MATERIAL_NODE: ' + node.get_class().get_name() + ' outputs=' + str(lib.get_material_expression_output_names(node)))
    if isinstance(node, unreal.MaterialExpressionCustom):
        sources = lib.get_inputs_for_material_expression(material, node)
        unreal.log('FE_AMMO_CUSTOM_SOURCES: ' + str(sources))
        assert len(sources) == 6 and all(sources)
        for element in node.get_editor_property('inputs'):
            unreal.log('FE_AMMO_CUSTOM_INPUT: ' + str(element.get_editor_property('input_name')))
for prop in (unreal.MaterialProperty.MP_EMISSIVE_COLOR, unreal.MaterialProperty.MP_OPACITY):
    unreal.log('FE_AMMO_MATERIAL_CONNECTION: ' + str(prop) + ' -> ' + str(lib.get_material_property_input_node(material, prop)))
player = unreal.load_asset('/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter')
equipment = unreal.get_default_object(player.generated_class()).get_component_by_class(unreal.FE_EquipmentComponent)
assert 'WBP_FE_AmmoRadial_C' in str(equipment.get_editor_property('ammo_selection_widget_class'))
unreal.log('FE_AMMO_WBP_VERIFIED: designer bindings, material brush and saved player linkage')
