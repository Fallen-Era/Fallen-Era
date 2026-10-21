"""Creates only generic radial designer assets. Existing ammo/player/weapon assets are not rewritten."""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
folder = '/Game/Combat/UI/WBP'
material = unreal.load_asset('/Game/Combat/UI/Materials/M_UI_AmmoSector')
assert material, 'Existing count-driven sector material is required'


def blueprint(name, parent):
    path = folder + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path), False
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', parent)
    return tools.create_asset(name, folder, unreal.WidgetBlueprint, factory), True


def widget(tree, cls, name):
    obj = unreal.new_object(cls, outer=tree, name=name)
    obj.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    return obj


def fill(canvas, child):
    slot = canvas.add_child_to_canvas(child)
    slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0, 0), maximum=unreal.Vector2D(1, 1)))
    slot.set_offsets(unreal.Margin())


def text(tree, name, label, font_size):
    result = widget(tree, unreal.TextBlock, name)
    result.set_text(unreal.Text(label))
    result.set_editor_property('justification', unreal.TextJustify.CENTER)
    font = result.get_editor_property('font')
    font.set_editor_property('size', font_size)
    result.set_font(font)
    result.set_editor_property('wrap_text_at', 130.0)
    return result


entry, created = blueprint('WBP_FE_RadialOption', unreal.FE_RadialMenuOptionWidget)
if created:
    tree = unreal.find_object(entry, 'WidgetTree')
    root = widget(tree, unreal.CanvasPanel, 'CanvasPanel_Root')
    sector = widget(tree, unreal.Image, 'Image_Sector')
    brush = sector.get_editor_property('brush')
    brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
    sector.set_brush(brush)
    sector.set_brush_from_material(material)
    fill(root, sector)
    content = widget(tree, unreal.Border, 'Border_Content')
    content.set_brush_color(unreal.LinearColor(0, 0, 0, 0))
    slot = root.add_child_to_canvas(content)
    slot.set_auto_size(True)
    slot.set_alignment(unreal.Vector2D(0.5, 0.5))
    labels = widget(tree, unreal.VerticalBox, 'VerticalBox_Content')
    content.set_content(labels)
    icon_size = widget(tree, unreal.SizeBox, 'SizeBox_Icon')
    icon_size.set_width_override(32)
    icon_size.set_height_override(32)
    icon = widget(tree, unreal.Image, 'Image_Icon')
    icon.set_visibility(unreal.SlateVisibility.COLLAPSED)
    icon_size.set_content(icon)
    labels.add_child_to_vertical_box(icon_size)
    for name, value, size in (('Text_Label', '항목', 16), ('Text_Secondary', '', 14), ('Text_Detail', '', 14), ('Text_Marker', '', 11)):
        labels.add_child_to_vertical_box(text(tree, name, value, size))
    assert unreal.FE_AmmoWidgetEditorLibrary.set_designer_root(entry, root)
    unreal.BlueprintEditorLibrary.compile_blueprint(entry)
    unreal.EditorAssetLibrary.save_loaded_asset(entry, only_if_is_dirty=False)

wheel, created = blueprint('WBP_FE_RadialMenu', unreal.FE_RadialMenuWidget)
if created:
    tree = unreal.find_object(wheel, 'WidgetTree')
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
    canvas = widget(tree, unreal.CanvasPanel, 'CanvasPanel_Wheel')
    size.set_content(canvas)
    fill(canvas, widget(tree, unreal.CanvasPanel, 'CanvasPanel_Options'))
    center = widget(tree, unreal.VerticalBox, 'VerticalBox_Center')
    slot = canvas.add_child_to_canvas(center)
    slot.set_auto_size(True)
    slot.set_alignment(unreal.Vector2D(0.5, 0.5))
    slot.set_position(unreal.Vector2D(250, 250))
    for name, value, size in (('Text_Title', '메뉴', 21), ('Text_SelectedItem', '', 12), ('Text_Hint', '', 12)):
        center.add_child_to_vertical_box(text(tree, name, value, size))
    assert unreal.FE_AmmoWidgetEditorLibrary.set_designer_root(wheel, root)
    unreal.BlueprintEditorLibrary.compile_blueprint(wheel)
    defaults = unreal.get_default_object(wheel.generated_class())
    defaults.set_editor_property('option_widget_class', entry.generated_class())
    defaults.set_editor_property('is_focusable', False)
    unreal.EditorAssetLibrary.save_loaded_asset(wheel, only_if_is_dirty=False)

# Validate old assets against inherited bindings without overwriting their saved designs.
for path in ('/Game/Combat/UI/WBP/WBP_FE_AmmoOption', '/Game/Combat/UI/WBP/WBP_FE_AmmoRadial'):
    bp = unreal.load_asset(path)
    assert bp, path
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.log('FE_GENERIC_RADIAL_READY: WBP_FE_RadialMenu and WBP_FE_RadialOption')
