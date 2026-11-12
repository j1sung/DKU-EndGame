"""현재 놀이동산 배치를 보존하며 대관람차를 고정 지지대와 회전부로 나눈다."""
import json
import math
import sys
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Round4Wheel';OUT.mkdir(parents=True,exist_ok=True)
BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round4';MAP='/Game/Map/L_Round4_Amusement'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors=list(api.get_all_level_actors())
assert not any(a.get_actor_label()=='R4A_FerrisWheel' for a in actors),'이미 회전 대관람차가 있습니다.'
frame=next(a for a in actors if a.get_actor_label().startswith('R4A_FerrisFrame_4000'))
cabins=[a for a in actors if a.get_actor_label().startswith('R4A_FerrisCabin_')];assert len(cabins)==16
rig=next(a for a in actors if a.get_actor_label()=='R4A_SeesawRig')
rig_values={k:rig.get_editor_property(k) for k in ['maximum_tilt_angle','push_interval','initial_delay','tilt_travel_time','tilt_hold_time','boarding_travel_time']}
transforms={a.get_path_name():a.get_actor_transform() for a in actors if a not in cabins}
sys.dont_write_bytecode=True
sys.path.insert(0,str(ROOT/'Tools/Editor'))
import generate_round4_amusement as g
g.OUT=OUT/'Sources';g.OUT.mkdir(parents=True,exist_ok=True);g.manifest={'meshes':[]}
(g.OUT/'round4.mtl').write_text('\n'.join('newmtl %s\nKd %s'%(k,' '.join(map(str,v))) for k,v in g.PALETTE.items()),encoding='utf-8')
m=g.Mesh('SM_R4A_FerrisSupport')
for y in [-290,290]:
    for x in [-900,900]:
        m.rod((x,y*2,0),(0,y,2350),60,'Cream',12)
        m.box((x,y*2,30),(300,300,60),'Cream')
m.save()
m=g.Mesh('SM_R4A_FerrisRotor')
for y in [-290,290]:
    for i in range(80):
        a,b=i*math.tau/80,(i+1)*math.tau/80
        m.rod((2000*math.cos(a),y,2000*math.sin(a)),(2000*math.cos(b),y,2000*math.sin(b)),15,'Coral',8)
    for i in range(16):
        a=i*math.tau/16;m.rod((0,y,0),(2000*math.cos(a),y,2000*math.sin(a)),10,'Cream',8)
m.rod((0,-340,0),(0,340,0),150,'Teal',40);m.save()
tools=unreal.AssetToolsHelpers.get_asset_tools();mesh_api=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for row in g.manifest['meshes']:
    assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/'+row['name'])
    t=unreal.AssetImportTask();t.filename=str(g.OUT/(row['name']+'.obj'));t.destination_path=BASE;t.destination_name=row['name'];t.automated=True
    opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;t.options=opt
    tools.import_asset_tasks([t]);mesh=unreal.load_asset(BASE+'/'+row['name']);assert mesh
    for i,slot in enumerate(mesh.static_materials):mesh.set_material(i,unreal.load_asset(BASE+'/Materials/M_R4A_'+str(slot.material_slot_name)))
    mesh_api.remove_collisions(mesh);ns=mesh_api.get_nanite_settings(mesh);ns.set_editor_property('enabled',True);mesh_api.set_nanite_settings(mesh,ns,True)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
cls=unreal.load_class(None,'/Script/DKUEndGame.AmusementFerrisWheel');assert cls
f=unreal.BlueprintFactory();f.set_editor_property('parent_class',cls)
bp=tools.create_asset('BP_R4A_FerrisWheel',BASE+'/Blueprints',unreal.Blueprint,f);assert bp
cdo=unreal.get_default_object(bp.generated_class());cdo.set_editor_property('wheel_mesh',unreal.load_asset(BASE+'/SM_R4A_FerrisRotor'));cdo.set_editor_property('cabin_mesh',unreal.load_asset(BASE+'/SM_R4A_FerrisCabin'))
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
pose=frame.get_actor_transform()
center=unreal.MathLibrary.transform_location(pose,unreal.Vector(0,0,2350))
wheel=api.spawn_actor_from_class(bp.generated_class(),center,frame.get_actor_rotation());wheel.set_actor_scale3d(frame.get_actor_scale3d());wheel.set_actor_label('R4A_FerrisWheel');wheel.set_folder_path('Round4Amusement/Background')
frame.static_mesh_component.set_static_mesh(unreal.load_asset(BASE+'/SM_R4A_FerrisSupport'))
frame.set_actor_label('R4A_FerrisSupport')
for a in cabins:api.destroy_actor(a)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
after={a.get_path_name():a for a in api.get_all_level_actors()}
for path,t in transforms.items():
    a=after[path];current=a.get_actor_transform()
    assert current.translation.distance(t.translation)<.01,path
    assert current.scale3d.distance(t.scale3d)<.0001,path
    q,r=current.rotation.to_tuple(),t.rotation.to_tuple()
    assert min(math.dist(q,r),math.dist(q,[-v for v in r]))<.0001,path
rig=next(a for a in after.values() if a.get_actor_label()=='R4A_SeesawRig')
assert rig_values=={k:rig.get_editor_property(k) for k in rig_values}
(OUT/'applied.json').write_text(json.dumps(dict(rig_values=rig_values,original_transforms_preserved=len(transforms),new_assets=['SM_R4A_FerrisSupport','SM_R4A_FerrisRotor','Blueprints/BP_R4A_FerrisWheel']),indent=2),encoding='utf-8')
unreal.log('ROUND4_FERRIS_APPLIED')
