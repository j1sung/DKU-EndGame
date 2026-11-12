"""현재 Round4 배치를 보존하고 밟으면 붕괴하는 부유 발판 세 개를 추가한다."""
import json
import math
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Round4Floating';BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round4';MAP='/Game/Map/L_Round4_Amusement'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors=list(api.get_all_level_actors());assert not any(a.get_actor_label().startswith('R4A_FloatingRefuge_') for a in actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
before=json.loads((OUT/'before_apply.json').read_text(encoding='utf-8'))
rig=next(a for a in actors if a.get_actor_label()=='R4A_SeesawRig')
assert before['rig']=={k:rig.get_editor_property(k) for k in before['rig']}
sys.dont_write_bytecode=True;sys.path.insert(0,str(ROOT/'Tools/Editor'))
import generate_round4_amusement as g
g.OUT=OUT/'Sources';g.OUT.mkdir(parents=True,exist_ok=True);g.manifest={'meshes':[]}
palette=dict(g.PALETTE,FloatingSurface=(.75,.72,.55))
(g.OUT/'round4.mtl').write_text('\n'.join('newmtl %s\nKd %s'%(k,' '.join(map(str,v))) for k,v in palette.items()),encoding='utf-8')
m=g.Mesh('SM_R4A_TemporaryPlatform_100x180');m.box((0,0,0),(100,180,24),'FloatingSurface')
m.taper((36,70),(50,90),-22,-12,'Cream')
for x in [-51.5,51.5]:m.box((x,0,-2),(3,180,24),'Teal')
for y in [-91.5,91.5]:m.box((0,y,-2),(100,3,24),'Teal')
for x in [-40,40]:
    for y in [-78,78]:m.sphere((x,y,-12),5,'Bulb')
m.rod((0,0,-25),(0,0,-22),28,'Teal',24);m.save('box',False)
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
mat=tools.create_asset('M_R4A_FloatingSurface',BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew());assert mat
e=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter);e.set_editor_property('parameter_name','PlatformTint');e.set_editor_property('default_value',unreal.LinearColor(.75,.72,.55,1));lib.connect_material_property(e,'',unreal.MaterialProperty.MP_BASE_COLOR)
for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,.9),(unreal.MaterialProperty.MP_SPECULAR,.2)]:
    e=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);e.set_editor_property('r',value);lib.connect_material_property(e,'',prop)
lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
name='SM_R4A_TemporaryPlatform_100x180';assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/'+name)
t=unreal.AssetImportTask();t.filename=str(g.OUT/(name+'.obj'));t.destination_path=BASE;t.destination_name=name;t.automated=True
opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;t.options=opt;tools.import_asset_tasks([t])
mesh=unreal.load_asset(BASE+'/'+name);assert mesh
for i,slot in enumerate(mesh.static_materials):
    mesh.set_material(i,mat if str(slot.material_slot_name)=='FloatingSurface' else unreal.load_asset(BASE+'/Materials/M_R4A_'+str(slot.material_slot_name)))
mesh_api=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);mesh_api.remove_collisions(mesh);mesh_api.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX);unreal.EditorAssetLibrary.save_loaded_asset(mesh)
f=unreal.BlueprintFactory();f.set_editor_property('parent_class',unreal.load_class(None,'/Script/DKUEndGame.TemporaryFloatingPlatform'))
bp=tools.create_asset('BP_R4A_TemporaryFloatingPlatform',BASE+'/Blueprints',unreal.Blueprint,f);assert bp
unreal.get_default_object(bp.generated_class()).set_editor_property('platform_mesh',mesh);unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
angle=math.radians(max(0,min(45,before['rig']['maximum_tilt_angle'])))
def height(x):return max(1190,math.ceil((850+125/math.cos(angle)+(abs(x)+53)*math.tan(angle)+62)/5)*5)
added=[];pose=rig.get_actor_transform()
for i,(x,y) in enumerate([(-180,-350),(180,350),(0,0)]):
    p=unreal.MathLibrary.transform_location(pose,unreal.Vector(x,y,height(x)-12))
    a=api.spawn_actor_from_class(bp.generated_class(),p,rig.get_actor_rotation());a.set_actor_scale3d(rig.get_actor_scale3d());a.set_actor_label('R4A_FloatingRefuge_'+str(i+1));a.set_folder_path('Round4Amusement/FloatingRefuges');added.append(a)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
after={a.get_path_name():a for a in api.get_all_level_actors()}
for row in before['actors']:
    a=after[row['path']]
    assert math.dist(a.get_actor_location().to_tuple(),row['loc'])<.001
    assert math.dist(a.get_actor_scale3d().to_tuple(),row['scale'])<.0001
    assert math.dist(a.get_actor_rotation().to_tuple(),row['rot'])<.0001
rig=next(a for a in after.values() if a.get_actor_label()=='R4A_SeesawRig')
assert before['rig']=={k:rig.get_editor_property(k) for k in before['rig']}
pads=[a for a in after.values() if a.get_actor_label().startswith('R4A_FloatingRefuge_')];assert len(pads)==3
assert all(a.get_editor_property('stand_duration')==3 for a in pads)
(OUT/'applied.json').write_text(json.dumps(dict(preserved=len(before['actors']),actors=len(after),rig=before['rig'],pads=[dict(label=a.get_actor_label(),loc=list(a.get_actor_location().to_tuple()),duration=a.get_editor_property('stand_duration')) for a in pads]),indent=2),encoding='utf-8')
unreal.log('ROUND4_FLOATING_PLATFORMS_ADDED')
