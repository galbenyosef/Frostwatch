"""Bake all combat limbs and glossy droplet material without touching the campaign map."""
import json
from pathlib import Path
import unreal as u
r=Path(u.Paths.project_dir()).resolve()
tools=u.AssetToolsHelpers.get_asset_tools();me=u.MaterialEditingLibrary
m=u.load_asset('/Game/Combat/M_LiquidBlood')
if not m:m=tools.create_asset('M_LiquidBlood','/Game/Combat',u.Material,u.MaterialFactoryNew())
me.delete_all_material_expressions(m)
c=me.create_material_expression(m,u.MaterialExpressionConstant3Vector);c.set_editor_property('constant',u.LinearColor(.19,.002,.001,1));me.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
f=me.create_material_expression(m,u.MaterialExpressionConstant);f.set_editor_property('r',.19);me.connect_material_property(f,'',u.MaterialProperty.MP_ROUGHNESS)
m.set_editor_property('used_with_instanced_static_meshes',True)
me.recompile_material(m);assert u.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert levels.load_level('/Game/Maps/Hearthmere')
u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),'fw.AuthorDamageParts')
report=json.loads((r/'Verification/fighting-authoring.json').read_text())
assert report=={'part_assets':20,'meshes':4},report
u.log('FIGHTING_ASSETS_READY '+str(report))
