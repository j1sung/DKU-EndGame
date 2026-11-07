"""회전판 장식의 형상을 메시로 고정해 재로딩 시 부착 변환 초기화에도 배치를 유지한다."""
import unreal,math,json
from pathlib import Path
MAP='/Game/Map/L_Lobby_Playground';BASE='/Game/LD_Gym/LDProps/StaticMeshes/Playground'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors={a.get_actor_label():a for a in api.get_all_level_actors()};turn=actors['BP_PhysicsObstacle_TurnTable'];disk=next(c for c in turn.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='Cylinder_0')
source=Path(unreal.Paths.project_saved_dir())/'PlaygroundRepair/MeshSources';source.mkdir(parents=True,exist_ok=True)
class Mesh:
 def __init__(self,name,color):self.name=name;self.color=color;self.points=[];self.faces=[]
 def face(self,points):
  n=len(self.points);self.points+=points
  for i in range(1,len(points)-1):self.faces.append([n,n+i,n+i+1])
 def rod(self,a,b,r):
  d=[b[i]-a[i] for i in range(3)];ln=math.sqrt(sum(x*x for x in d));d=[x/ln for x in d];q=[0,0,1] if abs(d[2])<.9 else [1,0,0]
  u=[d[1]*q[2]-d[2]*q[1],d[2]*q[0]-d[0]*q[2],d[0]*q[1]-d[1]*q[0]];ln=math.sqrt(sum(x*x for x in u));u=[x/ln for x in u];v=[d[1]*u[2]-d[2]*u[1],d[2]*u[0]-d[0]*u[2],d[0]*u[1]-d[1]*u[0]]
  rings=[[[p[j]+r*(u[j]*math.cos(i*math.tau/16)+v[j]*math.sin(i*math.tau/16)) for j in range(3)] for i in range(16)] for p in [a,b]]
  self.face(list(reversed(rings[0])));self.face(rings[1])
  for i in range(16):j=(i+1)%16;self.face([rings[0][i],rings[0][j],rings[1][j],rings[1][i]])
 def seat(self,c,angle):
  co,si=math.cos(angle),math.sin(angle)
  def point(i,j):
   lat=-math.pi/2+math.pi*i/8;lon=math.tau*j/16;u=55*math.cos(lat)*math.cos(lon);v=40*math.cos(lat)*math.sin(lon)
   return [c[0]+u*co-v*si,c[1]+u*si+v*co,c[2]+11*math.sin(lat)]
  for i in range(8):
   for j in range(16):
    a,b,c1,d=point(i,j),point(i,(j+1)%16),point(i+1,(j+1)%16),point(i+1,j)
    if i==0:self.face([a,c1,d])
    elif i==7:self.face([a,b,c1])
    else:self.face([a,b,c1,d])
 def save(self):
  lines=['o '+self.name,'mtllib roundabout.mtl','usemtl '+self.color]
  lines+=['v %.6f %.6f %.6f'%(x,-y,z) for x,y,z in self.points]
  lines+=['vt 0 0','vt 1 0','vt 1 1']
  for f in self.faces:lines.append('f '+' '.join('%d/%d'%(i+1,j+1) for j,i in enumerate(reversed(f))))
  path=source/(self.name+'.obj');path.write_text('\n'.join(lines));return path
parts={}
for i in range(6):
 a=i*math.tau/6;x=285*math.cos(a);y=285*math.sin(a);ex=135*math.cos(a);ey=135*math.sin(a)
 for role,color in [('Post','Yellow'),('Grip','Cream'),('Seat','Yellow')]:
  label='PG_Roundabout_%s_%02d'%(role,i);m=Mesh('SM_Playground_Roundabout_%s_%02d'%(role,i),color)
  if role=='Post':m.rod((x,y,10),(x,y,110),7)
  elif role=='Grip':m.rod((x,y,110),(ex,ey,110),7)
  else:m.seat((x,y,40),a)
  parts[label]=m
m=Mesh('SM_Playground_Roundabout_Hub','Teal');m.rod((0,0,10),(0,0,115),12);parts['PG_Roundabout_Hub']=m
(source/'roundabout.mtl').write_text('\n'.join('newmtl '+k+'\nKd .6 .6 .4' for k in ['Yellow','Cream','Teal']))
tasks=[]
for m in parts.values():
 path=m.save()
 if unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Meshes/'+m.name):continue
 t=unreal.AssetImportTask();t.filename=str(path);t.destination_path=BASE+'/Meshes';t.destination_name=m.name;t.automated=True;t.save=False
 opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;t.options=opt;tasks.append(t)
if tasks:unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
mesh_api=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for m in parts.values():
 mesh=unreal.load_asset(BASE+'/Meshes/'+m.name);assert mesh
 mat=unreal.load_asset(BASE+'/Materials/M_Playground_'+m.color);assert mat
 for i in range(len(mesh.static_materials)):mesh.set_material(i,mat)
 mesh_api.remove_collisions(mesh);mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
 unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
with unreal.ScopedEditorTransaction('Restore persistent playground roundabout geometry'):
 for label,m in parts.items():
  a=actors[label];a.modify();r=a.root_component;r.modify();r.set_editor_property('absolute_scale',True)
  r.set_editor_property('relative_location',unreal.Vector());r.set_editor_property('relative_rotation',unreal.Rotator());r.set_editor_property('relative_scale3d',unreal.Vector(1,1,1))
  c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset(BASE+'/Meshes/'+m.name))
  for i in range(c.get_num_materials()):c.set_material(i,None)
  c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 target=actors['BP_WobbleTarget2'];target.modify();target.root_component.modify();target.root_component.set_editor_property('absolute_scale',True);target.root_component.set_editor_property('relative_scale3d',unreal.Vector(1,1,1))
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
Path(unreal.Paths.project_saved_dir()+'PlaygroundRepair/mesh-parts.json').write_text(json.dumps({label:m.name for label,m in parts.items()},indent=2))
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(1450,-650,850),unreal.Rotator(pitch=-30,yaw=134,roll=0))
unreal.log('PLAYGROUND_PERSISTENT_MESHES_SAVED')
