"""높은 외벽과 2층 창문을 놀이터 색상·둥근 마감으로 구성한다. 원본 에셋은 수정하지 않는다."""
import unreal,json
from pathlib import Path
MAP='/Game/Map/L_LobbyJS_Playground'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
assert not any(k.startswith('PG_Enclosure_') for k in actors), '테마 외벽 장식이 이미 존재합니다.'
materials={k:unreal.load_asset('/Game/Map/Playground/Materials/M_Playground_'+k) for k in ['Mint','Cream','Teal','Coral','Yellow','Blue']}
meshes={k:unreal.load_asset('/Engine/BasicShapes/'+k) for k in ['Cube','Cylinder','Sphere']}
def paint(a,color):
 a.modify()
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  c.modify()
  for i in range(c.get_num_materials()): c.set_material(i,materials[color])
added=[]
def detail(label,mesh,loc,size,color):
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc))
 a.set_actor_label('PG_Enclosure_'+label); a.set_folder_path('Playground/Enclosure')
 c=a.static_mesh_component; c.set_static_mesh(meshes[mesh]); c.set_material(0,materials[color])
 c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 a.set_actor_scale3d(unreal.Vector(*(s/100 for s in size)))
 added.append(a)
 return a
with unreal.ScopedEditorTransaction('Theme tall playground walls and upstairs windows'):
 for label,a in actors.items():
  if label in ['SM_Dummy_Wall_4x4','SM_Dummy_Wall_4x5','SM_Dummy_Wall_4x6','SM_Dummy_Wall_4x7']: paint(a,'Mint')
  elif label in ['SM_Dummy_Wall_4x9','SM_Dummy_Wall_4x10','SM_Dummy_Wall_4x11','SM_Dummy_Wall_4x12']: paint(a,'Cream')
  elif label.startswith('SM_Dummy_Window_'): paint(a,'Teal')
 # 원기둥과 구형 캡으로 모서리를 마감하고 기존 벽의 충돌은 유지한다.
 for x in [-1480,1480]:
  for y in [-1980,1980]:
   detail('Corner','Cylinder',(x,y,535),(70,70,1070),'Teal')
   detail('CornerCap','Sphere',(x,y,1075),(100,100,100),'Yellow')
 for x in [-1480,1480]:
  detail('TopCap','Cube',(x,0,1080),(60,3980,30),'Yellow')
  detail('SoftBand','Cube',(x,0,400),(42,3960,32),'Coral')
 for y in [-1980,1980]:
  detail('TopCap','Cube',(0,y,1080),(3000,60,30),'Yellow')
  detail('SoftBand','Cube',(0,y,400),(2960,42,32),'Coral')
 # 2층 데크와 마주한 기존 창문 3개를 전망창으로 유지한다.
 for i,y in enumerate([-976,-1376,-1776]):
  detail('WindowSill_%d'%i,'Cube',(1455,y,775),(85,300,22),'Yellow')
 # 놀이 구역을 향한 벽에는 부드러운 색상의 원형 장식을 반복한다.
 for i,x in enumerate([-1100,-550,0,550,1100]):
  detail('WallDot_%d'%i,'Sphere',(x,1964,650),(180,28,180),['Coral','Yellow','Blue','Yellow','Coral'][i])
 for i,y in enumerate([-1450,-750,0,750,1450]):
  detail('WallDotSide_%d'%i,'Sphere',(-1465,y,650),(28,180,180),['Yellow','Coral','Blue','Coral','Yellow'][i])
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
# 저장 후 다시 조회하여 기존 그리드 재질이 외벽/창틀에 남지 않았는지 확인한다.
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
checked=[]
for label,a in actors.items():
 if label.startswith('SM_Dummy_Wall_') or label.startswith('SM_Dummy_Window_'):
  for c in a.get_components_by_class(unreal.StaticMeshComponent):
   for i in range(c.get_num_materials()):
    m=c.get_material(i)
    assert m and '/Game/Map/Playground/Materials/' in m.get_path_name(), label
  checked.append(label)
report={'map':MAP,'themed_walls_and_frames':len(checked),'added_details':len(added),'window_sets':3,'saved':True,'pie_tested':False}
Path(unreal.Paths.project_saved_dir()+'PlaygroundWork/enclosure-themed.json').write_text(json.dumps(report,indent=2))
unreal.log('PLAYGROUND_ENCLOSURE_THEMED')

