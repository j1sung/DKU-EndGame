"""기존 L_TestJS 배치를 보존하며 전용 도심 메시를 적용. Editor Python에서 실행."""
import unreal,json,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()); SRC=ROOT/'Saved/UrbanArenaWork/Source'
MAP='/Game/Map/L_TestJS'; BASE='/Game/Map/UrbanArena'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP,'L_TestJS에서만 실행합니다.'
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
assert not any(k.startswith('UA_') for k in actors),'이미 적용된 레벨입니다.'
assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Materials/M_Urban_Plaster'),'생성 에셋 덮어쓰기 금지'
manifest=json.loads((SRC/'manifest.json').read_text())
# 저장되지 않은 현재 배치까지 변환 스냅샷으로 기록한다. 원본 레벨 저장은 마지막에만 수행한다.
snapshot=[]
for a in actors.values():
 p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d()
 snapshot.append({'label':a.get_actor_label(),'path':a.get_path_name(),'loc':[p.x,p.y,p.z],'rot':[r.pitch,r.yaw,r.roll],'scale':[s.x,s.y,s.z]})
(ROOT/'Saved/UrbanArenaWork/before.json').write_text(json.dumps(snapshot,indent=2))
asset_tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
mats={}
def constant(m,value,x,y):
 e=lib.create_material_expression(m,unreal.MaterialExpressionConstant,x,y);e.set_editor_property('r',value);return e
for name,color in manifest['palette'].items():
 m=asset_tools.create_asset('M_Urban_'+name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 base=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-600,0);base.set_editor_property('constant',unreal.LinearColor(*color,1))
 noise=lib.create_material_expression(m,unreal.MaterialExpressionNoise,-650,180);noise.set_editor_property('scale',.035);noise.set_editor_property('levels',2);noise.set_editor_property('quality',1)
 mul=lib.create_material_expression(m,unreal.MaterialExpressionMultiply,-400,180);mul.set_editor_property('const_b',.055 if name not in ['Glass','GlassLight','Metal'] else .012);lib.connect_material_expressions(noise,'',mul,'A')
 add=lib.create_material_expression(m,unreal.MaterialExpressionAdd,-200,0);lib.connect_material_expressions(base,'',add,'A');lib.connect_material_expressions(mul,'',add,'B');lib.connect_material_property(add,'',unreal.MaterialProperty.MP_BASE_COLOR)
 rough=constant(m,.22 if 'Glass' in name else .42 if name=='Metal' else .86,-200,350);lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 if name=='Metal':lib.connect_material_property(constant(m,.65,-200,450),'',unreal.MaterialProperty.MP_METALLIC)
 if name=='Light':lib.connect_material_property(base,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 lib.recompile_material(m);mats[name]=m
# Batch import: explicit UVs and one object per file keep each mesh independently reusable.
tasks=[]
for item in manifest['meshes']:
 t=unreal.AssetImportTask();t.filename=str(SRC/(item['name']+'.obj'));t.destination_path=BASE+'/Meshes';t.destination_name=item['name'];t.automated=True;t.save=False
 opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.static_mesh_import_data.combine_meshes=True;t.options=opt;tasks.append(t)
for i in range(6):
 t=unreal.AssetImportTask();t.filename=str(SRC/('T_Urban_Sign_%02d.png'%i));t.destination_path=BASE+'/Textures';t.automated=True;t.save=False;tasks.append(t)
asset_tools.import_asset_tasks(tasks)
for i in range(6):
 name='Sign%02d'%i;tex=unreal.load_asset(BASE+'/Textures/T_Urban_Sign_%02d'%i);assert tex
 m=asset_tools.create_asset('M_Urban_'+name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 e=lib.create_material_expression(m,unreal.MaterialExpressionTextureSample,-400,0);e.set_editor_property('texture',tex);lib.connect_material_property(e,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
 lib.connect_material_property(constant(m,.75,-200,200),'',unreal.MaterialProperty.MP_ROUGHNESS);lib.recompile_material(m);mats[name]=m
meshes={};meshsys=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for item in manifest['meshes']:
 m=unreal.load_asset(BASE+'/Meshes/'+item['name']);assert m,item['name']
 for idx,slot in enumerate(m.static_materials):
  key=str(slot.material_slot_name)
  assert key in mats,(item['name'],key)
  m.set_material(idx,mats[key])
 if item.get('collision'):meshsys.remove_collisions(m);meshsys.add_simple_collisions(m,unreal.ScriptingCollisionShapeType.BOX)
 if item['name']=='SM_Urban_Grandstand':m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 if item.get('replace') or item['name']=='SM_Urban_PlazaPaving':
  settings=meshsys.get_nanite_settings(m);settings.set_editor_property('enabled',True);meshsys.set_nanite_settings(m,settings,True)
 meshes[item['name']]=m
# Save only owned assets; unrelated dirty packages stay untouched.
for name in unreal.EditorAssetLibrary.list_assets(BASE,recursive=True,include_folder=False):
 if not name.endswith('SM_Urban_Probe'):unreal.EditorAssetLibrary.save_asset(name,only_if_is_dirty=False)
added=[]
def spawn(name,mesh,loc,yaw=0,collision=False,folder='Details'):
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(pitch=0,yaw=yaw,roll=0));a.set_actor_label('UA_'+name+'_%02d'%len(added));a.set_folder_path('UrbanArena/'+folder)
 a.static_mesh_component.set_static_mesh(meshes[mesh]);a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION);added.append(a);return a
def paint(a,key):
 a.modify()
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  c.modify()
  for i in range(c.get_num_materials()):c.set_material(i,mats[key])
with unreal.ScopedEditorTransaction('Detail urban sparring plaza'):
 for item in manifest['meshes']:
  if 'replace' not in item:continue
  a=actors[item['replace']];origin,extent=a.get_actor_bounds(False);a.modify();c=a.static_mesh_component;c.modify();c.set_static_mesh(meshes[item['name']])
  # 기존 그레이박스의 중심·바닥·외곽 치수를 새 메시의 기준점으로 사용한다.
  a.set_actor_location(unreal.Vector(origin.x,origin.y,origin.z-extent.z),False,False);a.set_actor_scale3d(unreal.Vector(1,1,1))
  for i in range(c.get_num_materials()):c.set_material(i,None)
  a.set_folder_path('UrbanArena/Buildings')
 for label,x,y,yaw in [('Cube36',-1250,610,90),('Cube37',1400,610,-90),('Cube38',60,2100,0),('Cube39',60,-970,180)]:
  a=actors[label];a.modify();c=a.static_mesh_component;c.modify();c.set_static_mesh(meshes['SM_Urban_Grandstand']);a.set_actor_location(unreal.Vector(x,y,0),False,False);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False);a.set_actor_scale3d(unreal.Vector(1,1,1))
  for i in range(c.get_num_materials()):c.set_material(i,None)
  a.set_folder_path('UrbanArena/Grandstands')
 paint(actors['Floor3'],'Asphalt');paint(actors['Floor_Arena_12m4'],'Mat');paint(actors['Cube35'],'Teal')
 for label,a in actors.items():
  if label.startswith('Paint_'):paint(a,'Cream')
  elif label.startswith('Ring_Post'):paint(a,'Metal')
  elif label.startswith('Ring_Rope'):paint(a,'Cream' if '_11' in label else 'Red' if '_12' in label else 'Teal')
  elif label.startswith('SM_Dummy_Stairs'):paint(a,'Concrete')
 spawn('ArenaApron','SM_Urban_ArenaApron',(50,580,0),folder='Ring')
 for x,y,yaw in [(-2100,-1600,0),(2100,-1600,180),(-2100,2700,0),(2100,2700,180),(-2700,500,0),(2600,500,180)]:
  spawn('Lamp','SM_Urban_StreetLamp',(x,y,0),yaw,folder='StreetFurniture');spawn('Bin','SM_Urban_Bin',(x+85,y+75,0),folder='StreetFurniture')
 for y in [-1350,2500]:
  for x in [-400,400]:spawn('Barrier','SM_Urban_Barrier',(x,y,0),folder='Grandstands')
 for x in [-1750,1900]:
  for y in [310,900]:spawn('Barrier','SM_Urban_Barrier',(x,y,0),90,folder='Grandstands')
 # Main plaza is a generated merged paving mesh, loaded below when present.
 if 'SM_Urban_PlazaPaving' in meshes:spawn('PlazaPaving','SM_Urban_PlazaPaving',(50,530,0),folder='Ground')
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP),'맵 저장 실패'
api.set_selected_level_actors([])
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-5700,-6800,6200),unreal.Rotator(pitch=-36,yaw=51,roll=0))
report={'map':MAP,'meshes':len(meshes),'materials':len(mats),'textures':6,'added':len(added),'actors':len(api.get_all_level_actors()),'saved':True,'pie_tested':False}
(ROOT/'Saved/UrbanArenaWork/applied.json').write_text(json.dumps(report,indent=2));unreal.log('URBAN_ARENA_APPLIED')
