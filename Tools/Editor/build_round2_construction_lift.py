"""Organic의 수직 승강판 인스턴스에 편집 가능한 공사용 리프트를 입힌다."""
import importlib.util
import json
import math
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());WORK=ROOT/'Saved/RooftopLift';SRC=WORK/'Sources'
BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round2Rooftop';MAP='/Game/Map/L_Round2_Organic'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
assert not any(k.startswith('RT_Lift_') for k in actors),'이미 리프트 테마가 있습니다.'
before=json.loads((WORK/'before.json').read_text())
assert len(actors)==len(before['actors'])
lift=actors['BP_PhysicsObstacle_MovingPlatform']
platform=next(c for c in lift.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='Platform')
o,e=platform.get_owner().get_actor_bounds(False)
assert abs(e.x-585)<1 and abs(e.y-585)<1, '현재 승강판 크기에 맞는지 확인하세요.'
center=(o.x,o.y);floor=o.z+e.z
movement=lift.get_component_by_class(unreal.InterpToMovementComponent)
assert abs(movement.get_editor_property('control_points')[1].position_control_point.z-1600)<.01
SRC.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('organic_mesh',ROOT/'Tools/Editor/round2_organic_geometry.py')
geo=importlib.util.module_from_spec(spec);spec.loader.exec_module(geo)
definitions=[]


class Mesh(geo.Mesh):
 def __init__(self,name):
  super().__init__(name);self.materials=[];self.material='Steel'
 def face(self,points):
  old=len(self.faces);super().face(points);self.materials.extend([self.material]*(len(self.faces)-old))
 def box(self,c,s,mat='Steel'):
  self.material=mat;super().box(c,s)
 def rod(self,a,b,r,mat='Steel',sides=10):
  self.material=mat;super().rod(a,b,r,sides)
 def save(self,collision=False):
  lines=['mtllib lift.mtl','o '+self.name]+['v %.5f %.5f %.5f'%(x,-y,z) for x,y,z in self.points]
  lines+=['vt 0 0','vt 1 0','vt 1 1'];last=None
  for f,mat in sorted(zip(self.faces,self.materials),key=lambda v:v[1]):
   if mat!=last:lines.append('usemtl '+mat);last=mat
   lines.append('f '+' '.join('%d/%d'%(idx+1,i+1) for i,idx in enumerate(reversed(f))))
  (SRC/(self.name+'.obj')).write_text('\n'.join(lines));definitions.append({'name':self.name,'collision':collision})


# 가림판 한 장이 하나의 메시/충돌/액터다. 발끝막이 외에는 망 사이가 보인다.
m=Mesh('SM_RT_Lift_GuardPanel_240')
for x in [-117,117]:
 m.box((x,0,90),(6,8,180),'LiftSafety');m.box((x,0,5),(18,20,10),'Steel')
for z in [27,95,178]:m.box((0,0,z),(240,6,6),'LiftSafety')
m.box((0,0,13),(234,5,26),'LiftSafety')
for x in range(-90,91,30):m.rod((x,0,30),(x,0,176),1.1,'Zinc',6)
for z in [55,125,150]:m.rod((-114,0,z),(114,0,z),1.1,'Zinc',6)
for x in [-109,109]:
 for z in [38,166]:m.rod((x,-4,z),(x,-7,z),2.6,'Steel',8)
m.save(True)

m=Mesh('SM_RT_Lift_CabFrame')
for x in [-568,568]:
 m.box((x,0,-11),(22,1170,22),'Steel')
 for y in [-568,568]:
  m.box((x,y,115),(12,12,230),'Steel');m.box((x,y,233),(24,24,8),'LiftSafety')
for y in [-568,568]:
 m.box((0,y,-11),(1170,22,22),'Steel')
 if y<0:m.box((0,y,230),(1150,12,12),'LiftSafety')
# 열린 북·동쪽 입구에는 머리 위 가로대도 두지 않는다.
for x in [-568]:m.box((x,0,230),(12,1150,12),'LiftSafety')
for y in range(-480,481,160):m.box((0,y,-18),(1130,13,18),'Steel')
for x in range(-530,531,65):
 for y in [-579,579]:m.box((x,y,-7),(28,3,10),'LiftSafety')
m.save()

m=Mesh('SM_RT_Lift_DeckSkin');m.box((0,0,1),(1138,1138,2),'Steel')
for x in range(-545,546,55):
 m.box((x,0,2.5),(2.5,1100,1.5),'Zinc')
for y in [-535,535]:m.box((0,y,3),(1100,12,1),'LiftSafety')
m.save()

m=Mesh('SM_RT_Lift_MastModule_300')
for x in [-30,30]:
 for y in [-30,30]:m.box((x,y,150),(10,10,300),'Steel')
for z in [0,100,200,300]:
 m.box((0,0,z),(76,76,8),'Steel')
for z in [0,100,200]:
 for y in [-30,30]:
  m.rod((-30,y,z+8),(30,y,z+92),3,'Zinc',6);m.rod((30,y,z+8),(-30,y,z+92),3,'Zinc',6)
 for x in [-30,30]:m.rod((x,-30,z+8),(x,30,z+92),3,'Zinc',6)
m.box((0,37,150),(18,10,300),'Zinc')
for z in range(8,300,16):m.box((0,45,z),(23,10,6),'Steel')
for x in [-25,25]:
 for y in [-25,25]:m.rod((x,y,302),(x,y,307),3,'Zinc',8)
m.save(True)

m=Mesh('SM_RT_Lift_MastFoot');m.box((0,0,12),(145,145,24),'Steel')
for x in [-55,55]:
 for y in [-55,55]:m.rod((x,y,24),(x,y,32),5,'Zinc',8)
m.save(True)

m=Mesh('SM_RT_Lift_HeadBeam')
for z in [-24,24]:m.box((0,0,z),(900,70,12),'LiftSafety')
m.box((0,0,0),(900,10,48),'Steel')
for x in [-420,-320,0,320,420]:m.box((x,0,0),(10,70,55),'Steel')
for x in [-320,320]:m.rod((x,-12,-25),(x,-12,-50),12,'Zinc',16)
m.save()

m=Mesh('SM_RT_Lift_Winch')
m.box((0,0,12),(170,120,24),'Steel');m.box((-43,0,70),(74,98,105),'Teal')
m.rod((0,-45,70),(0,45,70),42,'Steel',24)
for y in [-48,48]:m.rod((0,y-4,70),(0,y+4,70),49,'LiftSafety',24)
for y in range(-35,36,8):m.rod((0,y,70),(0,y+2,70),43,'Zinc',24)
m.box((55,0,66),(45,80,65),'Zinc')
for z in range(45,85,10):m.box((79,0,z),(4,65,3),'Steel')
m.save(True)

m=Mesh('SM_RT_Lift_Carriage')
m.box((0,0,100),(75,16,155),'LiftSafety')
for z in [45,155]:
 for x in [-40,40]:m.rod((x,-13,z),(x,13,z),17,'Steel',20)
m.box((0,28,100),(38,60,85),'Teal');m.save()

m=Mesh('SM_RT_Lift_ControlBox');m.box((0,0,60),(48,30,100),'Teal')
m.box((0,-17,65),(40,5,78),'Ivory')
for z,r,mat in [(80,7,'LiftSafety'),(55,6,'Steel')]:m.rod((0,-20,z),(0,-25,z),r,mat,16)
m.box((0,0,115),(57,42,8),'Steel');m.save()

(SRC/'lift.mtl').write_text('\n'.join('newmtl '+k+'\nKd .3 .3 .3' for k in ['Steel','LiftSafety','Zinc','Teal','Ivory']))
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Materials/M_RT_LiftSafety')
safety=tools.create_asset('M_RT_LiftSafety',BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
color=lib.create_material_expression(safety,unreal.MaterialExpressionConstant3Vector,-400,0);color.set_editor_property('constant',unreal.LinearColor(.58,.32,.055,1))
noise=lib.create_material_expression(safety,unreal.MaterialExpressionNoise,-600,180);noise.set_editor_property('scale',.08);noise.set_editor_property('levels',2)
mul=lib.create_material_expression(safety,unreal.MaterialExpressionMultiply,-400,180);mul.set_editor_property('const_b',.025);lib.connect_material_expressions(noise,'',mul,'A')
add=lib.create_material_expression(safety,unreal.MaterialExpressionAdd,-180,0);lib.connect_material_expressions(color,'',add,'A');lib.connect_material_expressions(mul,'',add,'B');lib.connect_material_property(add,'',unreal.MaterialProperty.MP_BASE_COLOR)
for value,prop,y in [(.67,unreal.MaterialProperty.MP_ROUGHNESS,300),(.25,unreal.MaterialProperty.MP_METALLIC,400)]:
 e=lib.create_material_expression(safety,unreal.MaterialExpressionConstant,-180,y);e.set_editor_property('r',value);lib.connect_material_property(e,'',prop)
lib.recompile_material(safety);assert unreal.EditorAssetLibrary.save_loaded_asset(safety)
tasks=[]
for item in definitions:
 assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Meshes/'+item['name'])
 t=unreal.AssetImportTask();t.filename=str(SRC/(item['name']+'.obj'));t.destination_path=BASE+'/Meshes';t.destination_name=item['name'];t.automated=True;t.save=False
 opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;t.options=opt;tasks.append(t)
tools.import_asset_tasks(tasks)
ms=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);meshes={}
for item in definitions:
 mesh=unreal.load_asset(BASE+'/Meshes/'+item['name']);assert mesh
 for i,slot in enumerate(mesh.static_materials):mesh.set_material(i,unreal.load_asset(BASE+'/Materials/M_RT_'+str(slot.material_slot_name)))
 ms.remove_collisions(mesh)
 if item['collision']:
  ms.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)
  mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
 assert unreal.EditorAssetLibrary.save_loaded_asset(mesh);meshes[item['name']]=mesh

added=[]
def spawn(kind,loc,yaw=0,scale=(1,1,1),moving=False,collision=False,label=None):
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(yaw=yaw))
 a.set_actor_label(label or 'RT_Lift_'+kind+'_%02d'%len(added));a.set_folder_path('Round2Rooftop/ConstructionLift/'+('MovingCab' if moving else 'FixedMast'))
 c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE if moving else unreal.ComponentMobility.STATIC)
 c.set_static_mesh(meshes['SM_RT_Lift_'+kind]);c.set_editor_property('use_default_collision',False)
 c.set_collision_profile_name('BlockAllDynamic' if collision and moving else 'BlockAll' if collision else 'NoCollision')
 c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION)
 c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE)
 if moving:
  # 승강판의 (11.7,11.7,.2) 스케일을 장식이 상속하면 철망이 찌그러진다.
  c.set_absolute(False,False,True)
  assert a.attach_to_component(lift.root_component,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
 a.set_actor_scale3d(unreal.Vector(*scale))
 if moving:
  # Editor의 부착 액터 저장 시 기본 상대 변환으로 되돌아가지 않게 명시한다.
  a.modify();c.modify()
  c.set_editor_property('relative_location',c.get_editor_property('relative_location'))
  c.set_editor_property('relative_rotation',c.get_editor_property('relative_rotation'))
  c.set_editor_property('relative_scale3d',unreal.Vector(*scale))
 added.append(a);return a

cx,cy=center
with unreal.ScopedEditorTransaction('Build modular open construction lift on existing moving deck'):
 platform.modify();platform.set_material(0,unreal.load_asset(BASE+'/Materials/M_RT_Steel'))
 spawn('CabFrame',(cx,cy,floor),moving=True)
 spawn('DeckSkin',(cx,cy,floor+.3),moving=True)
 # 남·서쪽은 네모판 5개씩, 북·동쪽은 코너만 남겨 위·아래 승하차 입구를 연다.
 for side in ['South','West','East','North']:
  for j in range(5):
   if side in ['North','East'] and j not in [0,4]:continue
   u=-468+j*234
   if side in ['South','North']:x,y,yaw=cx+u,cy+(-568 if side=='South' else 568),0
   else:x,y,yaw=cx+(-568 if side=='West' else 568),cy+u,90
   spawn('GuardPanel_240',(x,y,floor),yaw,(.975,1,1),True,True,'RT_Lift_Panel_'+side+'_%02d'%j)
 for dx in [-320,320]:
  spawn('Carriage',(cx+dx,cy-584,floor),moving=True)
  x,y=cx+dx,cy-650
  spawn('MastFoot',(x,y,965))
  for i in range(7):spawn('MastModule_300',(x,y,965+i*300),collision=True)
 spawn('HeadBeam',(cx,cy-650,3065))
 spawn('Winch',(cx+440,cy-650,989),collision=True)
 spawn('ControlBox',(cx+542,cy+430,floor+12),90,moving=True)
 # 원래 플랫폼/이동 제어점/나머지 사용자 배치는 바꾸지 않는다.
 for row in before['actors']:
  a=actors[row['label']];p,r,s=a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()
  assert math.dist([p.x,p.y,p.z],row['loc'])<.01 and math.dist([r.pitch,r.yaw,r.roll],row['rot'])<.01 and math.dist([s.x,s.y,s.z],row['scale'])<.001,row['label']
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
report={'meshes':definitions,'added':len(added),'actors':len(api.get_all_level_actors()),'moving':[a.get_actor_label() for a in added if a.get_attach_parent_actor()==lift],
        'floor_z':floor,'center':[cx,cy],'entrance':'North / +Y and East / +X; center 702cm open','preserved_actors':len(before['actors']),'pie_tested':False}
(WORK/'applied.json').write_text(json.dumps(report,indent=2));unreal.log('CONSTRUCTION_LIFT_SAVED '+str(len(added)))
