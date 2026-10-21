"""Create designer-editable, BindWidget-backed ammo WBP assets with a UI sector material.
Run in a saved/closed editor via PythonScript commandlet. Does not reset ammo/weapon test values.
"""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
folder = '/Game/Combat/UI/WBP'
material_path = '/Game/Combat/UI/Materials/M_UI_AmmoSector'

material = unreal.load_asset(material_path) if unreal.EditorAssetLibrary.does_asset_exist(material_path) else None
if not material:
    material = tools.create_asset('M_UI_AmmoSector', '/Game/Combat/UI/Materials', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    lib = unreal.MaterialEditingLibrary
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, 0, 0)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ('UV', 'SectorStart', 'SectorAngle', 'InnerRadius', 'OuterRadius', 'Tint'):
        element = unreal.CustomInput()
        element.set_editor_property('input_name', name)
        inputs.append(element)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', '''
float2 p = UV - 0.5;
float r = length(p);
float a = atan2(p.x, -p.y);
if (a < 0) a += 6.28318530718;
float d = fmod(a - SectorStart + 6.28318530718, 6.28318530718);
float mask = step(InnerRadius, r) * step(r, OuterRadius) * step(0.022, d) * step(d, SectorAngle - 0.022);
return float4(Tint.rgb, mask);
''')
    lib.connect_material_expressions(uv, '', custom, 'UV')
    for index, (name, value) in enumerate((('SectorStart', 0.0), ('SectorAngle', 3.14159265), ('InnerRadius', 0.17), ('OuterRadius', 0.45))):
        node = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 140 + index * 100)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', value)
        lib.connect_material_expressions(node, '', custom, name)
    color = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, 600)
    color.set_editor_property('parameter_name', 'Tint')
    color.set_editor_property('default_value', unreal.LinearColor(0.055, 0.075, 0.1, 1.0))
    lib.connect_material_expressions(color, '', custom, 'Tint')
    alpha = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 200, 160)
    for channel in ('r', 'g', 'b'):
        alpha.set_editor_property(channel, False)
    alpha.set_editor_property('a', True)
    lib.connect_material_expressions(custom, '', alpha, '')
    lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)


def blueprint(name, parent):
    path = folder + '/' + name
    bp = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not bp:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        bp = tools.create_asset(name, folder, unreal.WidgetBlueprint, factory)
    return bp


def widget(tree, cls, name):
    obj = unreal.find_object(tree, name)
    if obj is None:
        obj = unreal.new_object(cls, outer=tree, name=name)
    if isinstance(obj, unreal.PanelWidget):
        obj.clear_children()
    if isinstance(obj, unreal.Image):
        brush = obj.get_editor_property('brush')
        brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
        obj.set_brush(brush)
    obj.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    return obj


def fill(canvas, child):
    slot = canvas.add_child_to_canvas(child)
    slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0, 0), maximum=unreal.Vector2D(1, 1)))
    slot.set_offsets(unreal.Margin())
    return slot


def text(tree, name, value, size=14):
    result = widget(tree, unreal.TextBlock, name)
    result.set_text(unreal.Text(value))
    result.set_editor_property('justification', unreal.TextJustify.CENTER)
    font = result.get_editor_property('font')
    font.set_editor_property('size', size)
    result.set_font(font)
    result.set_editor_property('wrap_text_at', 130.0)
    return result


entry = blueprint('WBP_FE_AmmoOption', unreal.FE_AmmoOptionWidget)
tree = unreal.find_object(entry, 'WidgetTree')
assert tree, 'Designer WidgetTree missing'
root = widget(tree, unreal.CanvasPanel, 'CanvasPanel_Root')
sector = widget(tree, unreal.Image, 'Image_Sector')
sector.set_brush_from_material(material)
fill(root, sector)
content = widget(tree, unreal.Border, 'Border_Content')
content.set_brush_color(unreal.LinearColor(0, 0, 0, 0))
slot = root.add_child_to_canvas(content)
slot.set_auto_size(True)
slot.set_alignment(unreal.Vector2D(0.5, 0.5))
slot.set_position(unreal.Vector2D(400, 250))
labels = widget(tree, unreal.VerticalBox, 'VerticalBox_Content')
content.set_content(labels)
icon_size = widget(tree, unreal.SizeBox, 'SizeBox_Icon')
icon_size.set_width_override(32)
icon_size.set_height_override(32)
icon = widget(tree, unreal.Image, 'Image_AmmoIcon')
icon.set_visibility(unreal.SlateVisibility.COLLAPSED)
icon_size.set_content(icon)
labels.add_child_to_vertical_box(icon_size)
for name, value, size in (('Text_AmmoName', '탄약', 16), ('Text_Count', '0발', 14), ('Text_Damage', '피해 ×1', 14), ('Text_Loaded', '장전 중인 탄약', 11)):
    label = text(tree, name, value, size)
    labels.add_child_to_vertical_box(label)
assert unreal.FE_AmmoWidgetEditorLibrary.set_designer_root(entry, root)
unreal.BlueprintEditorLibrary.compile_blueprint(entry)
unreal.EditorAssetLibrary.save_loaded_asset(entry, only_if_is_dirty=False)

wheel = blueprint('WBP_FE_AmmoRadial', unreal.FE_AmmoRadialWidget)
tree = unreal.find_object(wheel, 'WidgetTree')
assert tree, 'Designer WidgetTree missing'
root = widget(tree, unreal.CanvasPanel, 'CanvasPanel_Root')
root.set_visibility(unreal.SlateVisibility.VISIBLE)
backdrop = widget(tree, unreal.Border, 'Border_Background')
backdrop.set_brush_color(unreal.LinearColor(0, 0, 0, 0.4))
backdrop.set_visibility(unreal.SlateVisibility.VISIBLE)
fill(root, backdrop)
size = widget(tree, unreal.SizeBox, 'SizeBox_Wheel')
size.set_width_override(500)
size.set_height_override(500)
slot = root.add_child_to_canvas(size)
slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0.5, 0.5), maximum=unreal.Vector2D(0.5, 0.5)))
slot.set_alignment(unreal.Vector2D(0.5, 0.5))
slot.set_auto_size(True)
slot.set_position(unreal.Vector2D(0, 0))
wheel_canvas = widget(tree, unreal.CanvasPanel, 'CanvasPanel_Wheel')
size.set_content(wheel_canvas)
options = widget(tree, unreal.CanvasPanel, 'CanvasPanel_Options')
fill(wheel_canvas, options)
center = widget(tree, unreal.VerticalBox, 'VerticalBox_Center')
slot = wheel_canvas.add_child_to_canvas(center)
slot.set_auto_size(True)
slot.set_alignment(unreal.Vector2D(0.5, 0.5))
slot.set_position(unreal.Vector2D(250, 250))
for name, value, font_size in (('Text_Title', '탄약 선택', 21), ('Text_SelectedAmmo', '', 12), ('Text_Hint', 'R 해제: 취소', 12)):
    center.add_child_to_vertical_box(text(tree, name, value, font_size))
assert unreal.FE_AmmoWidgetEditorLibrary.set_designer_root(wheel, root)
unreal.BlueprintEditorLibrary.compile_blueprint(wheel)
defaults = unreal.get_default_object(wheel.generated_class())
defaults.set_editor_property('option_widget_class', entry.generated_class())
defaults.set_editor_property('is_focusable', False)
defaults.set_visibility(unreal.SlateVisibility.VISIBLE)
unreal.EditorAssetLibrary.save_loaded_asset(wheel, only_if_is_dirty=False)

player = unreal.load_asset('/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter')
equipment = unreal.get_default_object(player.generated_class()).get_component_by_class(unreal.FE_EquipmentComponent)
equipment.set_editor_property('ammo_selection_widget_class', wheel.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(player)
unreal.EditorAssetLibrary.save_loaded_asset(player, only_if_is_dirty=False)
unreal.log('FE_AMMO_WBP_CREATED: WBP_FE_AmmoRadial + WBP_FE_AmmoOption, player linked')
