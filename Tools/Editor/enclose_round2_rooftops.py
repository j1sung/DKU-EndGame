"""현재 플레이 발판의 외곽에 최고 옥상보다 높은 독립 펜스 패널을 추가한다(Editor Python)."""
import importlib.util
import json
import math
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());WORK=ROOT/'Saved/RooftopFence'
BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round2Rooftop'
MAP='/Game/Map/L_Round2_Organic'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
assert not any(k.startswith('RT_PerimeterFence_') for k in actors),'이미 외곽 펜스가 있습니다.'
before=json.loads((WORK/'before.json').read_text(encoding='utf-8'))
assert len(before['actors'])==len(actors)
for row in before['actors']:
 p=actors[row['label']].get_actor_location()
 assert math.dist([p.x,p.y,p.z],row['loc'])<.01,'조사 이후 배치 변경 감지'
spec=importlib.util.spec_from_file_location('organic_geometry',ROOT/'Tools/Editor/round2_organic_geometry.py')
geo=importlib.util.module_from_spec(spec);spec.loader.exec_module(geo)
triangles=[t for t in json.loads((ROOT/'Saved/Round2OrganicWork/geometry.json').read_text())['triangles'] if t['actor'].startswith('SM_Dummy_')]
rects=[]
for row in before['actors']:
 label=row['label']
 if not (label.startswith(('SM_Dummy_Block_','SM_Dummy_Stairs_')) or label=='SM_Dummy_Wall_4x7'):continue
 o,e=row['bounds'];rects.append({'actor':label,'x0':o[0]-e[0],'x1':o[0]+e[0],'y0':o[1]-e[1],'y1':o[1]+e[1],'top':o[2]+e[2]})
highest=max(r['top'] for r in rects if r['actor'].startswith('SM_Dummy_Block_'))
TOP=highest+400


def inside(x,y):
 return any(r['x0']-.01<=x<=r['x1']+.01 and r['y0']-.01<=y<=r['y1']+.01 for r in rects)


def floor_height(x,y):
 heights=[geo.height_at(t['p'],x,y) for t in triangles]
 heights=[h for h in heights if h is not None]
 assert heights,(x,y)
 return max(heights)


# 직사각형 보행면의 합집합 경계만 취한다. 폭 15cm 이하 그레이박스 이음매는
# 통과 가능한 내부 접합부로 보고 닫지 않는다. 허브와 승강장 입구는 내부에 남는다.
segments=[];seen=set()
for r in rects:
 edges=[((r['x0'],r['y0']),(r['x1'],r['y0']),(0,-1)),
        ((r['x0'],r['y1']),(r['x1'],r['y1']),(0,1)),
        ((r['x0'],r['y0']),(r['x0'],r['y1']),(-1,0)),
        ((r['x1'],r['y0']),(r['x1'],r['y1']),(1,0))]
 for a,b,n in edges:
  axis=0 if a[1]==b[1] else 1
  cuts=[a[axis],b[axis]]
  for other in rects:
   for value in [other['x0'],other['x1']] if axis==0 else [other['y0'],other['y1']]:
    if a[axis]<value<b[axis]:cuts.append(value)
  cuts=sorted(set(cuts))
  for lo,hi in zip(cuts,cuts[1:]):
   # 아주 짧은 그레이박스 이음매는 이웃 패널의 20cm 겹침으로 덮는다.
   # 이를 별도 열로 만들면 내부 접합점에 가는 펜스 기둥만 솟는다.
   if hi-lo<30:continue
   mid=[(a[0]+b[0])/2,(a[1]+b[1])/2];mid[axis]=(lo+hi)/2
   if inside(mid[0]+n[0]*15,mid[1]+n[1]*15):continue
   aa=list(a);bb=list(b);aa[axis]=lo;bb[axis]=hi
   key=tuple(round(v,1) for v in aa+bb)
   if key in seen:continue
   seen.add(key);segments.append({'a':aa,'b':bb,'normal':n,'actor':r['actor']})

panels=[]
for i,s in enumerate(segments):
 a,b,n=s['a'],s['b'],s['normal'];length=math.dist(a,b);count=math.ceil(length/240)
 tangent=[(b[k]-a[k])/length for k in range(2)]
 for col in range(count):
  aa=[a[k]+(b[k]-a[k])*col/count for k in range(2)]
  bb=[a[k]+(b[k]-a[k])*(col+1)/count for k in range(2)]
  heights=[]
  for t in [.02,.5,.98]:
   x,y=[aa[k]+(bb[k]-aa[k])*t-n[k]*2 for k in range(2)]
   heights.append(floor_height(x,y))
  bottom=min(heights)-25
  height=TOP-bottom;rows=math.ceil(height/360)
  # 이웃 경계의 모서리에 20cm씩 겹쳐 작은 탈출 틈도 막는다.
  extra_a=20 if col==0 else 0;extra_b=20 if col==count-1 else 0
  width=length/count+extra_a+extra_b
  xy=[(aa[k]+bb[k])/2+n[k]*20+tangent[k]*(extra_b-extra_a)/2 for k in range(2)]
  for row in range(rows):
   panels.append({'label':'RT_PerimeterFence_%03d_%02d_%02d'%(i,col,row),
                  'loc':[*xy,bottom+height*row/rows],'yaw':math.degrees(math.atan2(tangent[1],tangent[0])),
                  'scale':[width/240,1,(height/rows)/360],'width':width,'height':height/rows})

folder=WORK/'Sources';folder.mkdir(parents=True,exist_ok=True)
(folder/'organic.mtl').write_text('newmtl Neutral\nKd .1 .14 .14\n')
name='SM_RT_PerimeterFence_240x360'
assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Meshes/'+name)
m=geo.Mesh(name)
for x in [-120,120]:m.rod((x,0,0),(x,0,360),4)
for z in [0,180,360]:m.rod((-120,0,z),(120,0,z),3)
for x in [-90,-60,-30,0,30,60,90]:m.rod((x,0,0),(x,0,360),.8,4)
for z in [60,120,240,300]:m.rod((-120,0,z),(120,0,z),.8,4)
source=m.save(folder)
task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=BASE+'/Meshes';task.destination_name=name;task.automated=True;task.save=False
options=unreal.FbxImportUI();options.import_materials=False;options.import_textures=False;options.static_mesh_import_data.combine_meshes=True;task.options=options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=unreal.load_asset(BASE+'/Meshes/'+name);assert mesh
ms=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
ms.remove_collisions(mesh);ms.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)
mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
mesh.set_material(0,unreal.load_asset(BASE+'/Materials/M_RT_Steel'))
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
with unreal.ScopedEditorTransaction('Enclose playable rooftops with tall perimeter fence'):
 for panel in panels:
  a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*panel['loc']),unreal.Rotator(yaw=panel['yaw']))
  a.set_actor_label(panel['label']);a.set_folder_path('Round2Rooftop/PerimeterFence');a.set_actor_scale3d(unreal.Vector(*panel['scale']))
  c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_editor_property('use_default_collision',False)
  c.set_collision_profile_name('Custom');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
  c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
  for channel in [unreal.CollisionChannel.ECC_PAWN,unreal.CollisionChannel.ECC_PHYSICS_BODY]:c.set_collision_response_to_channel(channel,unreal.CollisionResponseType.ECR_BLOCK)
 for row in before['actors']:
  a=actors[row['label']];p,r,s=a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()
  assert math.dist([p.x,p.y,p.z],row['loc'])<.01 and math.dist([r.pitch,r.yaw,r.roll],row['rot'])<.01 and math.dist([s.x,s.y,s.z],row['scale'])<.001,row['label']
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
(WORK/'applied.json').write_text(json.dumps({'panels':panels,'segments':segments,'highest_roof':highest,'fence_top':TOP,'actors':len(api.get_all_level_actors())},indent=2))
unreal.log('ROOFTOP_PERIMETER_SAVED '+str(len(panels))+' panels; top '+str(TOP))
