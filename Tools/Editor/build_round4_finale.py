"""현재 저장된 Round3 기믹 규격을 보존하고 Round4 순환 전투 맵을 최초 생성한다."""
import unreal,math,json,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());WORK=ROOT/'Saved/Round4Work';WORK.mkdir(parents=True,exist_ok=True)
MAP='/Game/Map/L_Round4';SOURCE='/Game/Map/L_Round3';BASE='/Game/Map/Round4Draft'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.EditorAssetLibrary.does_asset_exist(MAP),'기존 Round4를 덮어쓰지 않습니다.'
source_file=ROOT/'Content/Map/L_Round3.umap';source_hash=hashlib.sha256(source_file.read_bytes()).hexdigest()
assert unreal.EditorLoadingAndSavingUtils.load_map(SOURCE)
source_actors=api.get_all_level_actors();original={a.get_name():a for a in source_actors}
# Blueprint와 부착 액터를 통째로 복제해 사용자 인스턴스 수정값을 보존한다.
copy=unreal.EditorAssetLibrary.duplicate_asset(SOURCE,MAP);assert copy,'맵 복제 실패'
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();actors=api.get_all_level_actors();by={a.get_actor_label():a for a in actors}
keep=set()
for a in actors:
 label=a.get_actor_label()
 if label.startswith(('R3L_Bridge','R3L_Jet','R3M_','R3A_Ball','R3A_Pendulum','R3A_Drop')) or isinstance(a,(unreal.PlayerStart,unreal.Character,unreal.Light,unreal.SkyAtmosphere,unreal.PostProcessVolume)) or label=='R3A_PreviewSky' or label in ['R3L_Ramp_West','R3L_Ramp_East','R3A_Arena_HillUp','R3A_Arena_HillDown']:
  keep.add(a.get_name())
preserved={a.get_name():{'label':a.get_actor_label(),'scale':[a.get_actor_scale3d().x,a.get_actor_scale3d().y,a.get_actor_scale3d().z]} for a in actors if a.get_name() in keep and not isinstance(a,(unreal.PlayerStart,unreal.Character,unreal.Light,unreal.SkyAtmosphere,unreal.PostProcessVolume))}
for a in actors:
 if a.get_name() not in keep:api.destroy_actor(a)
meshes={n:unreal.load_asset('/Engine/BasicShapes/'+n) for n in ['Cube','Cylinder']};mats={n:unreal.load_asset('/Game/Map/Round3Draft/Materials/M_R3_'+n) for n in ['Ground','Balance','Moving','Goal','Recovery','BridgeRope']}
def move(a,p):a.set_actor_location(unreal.Vector(*p),False,False)
def shape(label,pos,size,mat='Ground',kind='Cube',rot=None,parent=None,collision=True):
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),rot or unreal.Rotator());a.set_actor_label('R4_'+label);a.set_folder_path('Round4/'+label.split('_')[0]);c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE if parent else unreal.ComponentMobility.STATIC);c.set_static_mesh(meshes[kind]);c.set_material(0,mats[mat]);c.set_editor_property('use_default_collision',False);c.set_collision_profile_name(('BlockAllDynamic' if parent else 'BlockAll') if collision else 'NoCollision');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE)
 if parent:
  c.set_absolute(False,False,True);assert a.attach_to_component(parent.root_component,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
 a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)));return a
def floor(label,x,y,w,d,z=0,mat='Ground'):return shape(label,(x,y,z-20),(w,d,40),mat)
def slope(label,p0,p1,width):
 a,b=unreal.Vector(*p0),unreal.Vector(*p1);v=b-a;p=(a+b)*.5
 return shape(label,(p.x,p.y,p.z-20),(v.length()+12,width,40),'Balance',rot=unreal.MathLibrary.make_rot_from_x(v))
def stairs(label,p0,p1,width,rise,steps):
 a,b=unreal.Vector(*p0),unreal.Vector(*p1);v=b-a;run=unreal.Vector(v.x,v.y,0);angle=unreal.MathLibrary.make_rot_from_x(run)
 for i in range(steps):
  p=a+run*((i+.5)/steps);top=a.z+(i+1)*rise/steps;height=top-a.z
  shape(label+'_%02d'%i,(p.x,p.y,a.z+height/2),(run.length()/steps+1,width,height),'Balance',rot=angle)
# 36m 정방형 회수 바닥. 북쪽 상층 아래에는 얕은 구덩이와 복귀 경사를 둔다.
floor('Ground_West',-850,0,1900,3600);floor('Ground_East',1200,0,1200,3600)
floor('Ground_South',350,-400,500,2800);floor('Ground_North',350,1650,500,300)
floor('Pit_Bottom',350,1250,500,500,-120,'Recovery');slope('Pit_Exit',(350,1000,-120),(350,1500,0),480)
# 비대칭 네 테라스. 높은 남북 섬과 낮은 동서 섬을 외곽 루프로 연결한다.
floor('North_High',-400,1300,2400,600,600);floor('South_High',400,-1300,2400,600,600,'Balance')
floor('West_Mid',-1275,-950,650,900,300,'Goal');floor('East_Mid',1275,950,650,900,300,'Goal')
floor('West_Return',-1250,-250,650,600,300,'Goal');floor('East_Return',1250,250,650,600,300,'Goal')
floor('West_Dock',-875,0,350,500,300);floor('East_Dock',875,0,350,500,300)
stairs('West_Stairs',(-450,-850,0),(-950,-850,300),360,300,20)
stairs('East_Stairs',(450,850,0),(950,850,300),360,300,20)
slope('SW_Connection',(-1250,-1300,300),(-750,-1300,600),400)
slope('NE_Connection',(1250,1300,300),(750,1300,600),400)
floor('North_BridgeLanding',0,950,600,140,600);floor('South_BridgeLanding',0,-950,600,140,600,'Balance')
# 사용자 조절 경사로의 크기·기울기는 그대로 두고 새 남북 지형으로 평행 이동한다.
for side,target in [('West',(-1250,450)),('East',(1250,-450))]:
 ramp=by['R3L_Ramp_'+side];log=by['R3M_LogSpawner_'+side];old=ramp.get_actor_location();delta=unreal.Vector(target[0]-old.x,target[1]-old.y,0);ramp.set_actor_location(old+delta,False,False);log.set_actor_location(log.get_actor_location()+delta,False,False)
# 위층 다리 규격은 유지하며 남북으로 돌려, 중층 횡단 발판과 교차시킨다.
by['R3L_BridgeMotion'].set_actor_rotation(unreal.Rotator(yaw=90),False)
move(by['R3M_Sweeper'],(0,0,6))
move(by['R3A_Pendulum_Proxy'],(-300,700,140))
for i,p in enumerate([(650,-350,820),(-650,350,820)]):
 move(by['R3A_DropBlock_'+str(i)],p)
 shape('Hazard_DropMark_'+str(i),(p[0],p[1],2),(260,260,4),'Moving',collision=False)
for name,p in [('R3A_Ball_Proxy',(0,700,679)),('R3A_Ball_Proxy2',(0,0,679))]:move(by[name],p)
for i,(x,y) in enumerate([(-600,-700),(600,700),(0,-450)]):
 root=by['R3L_JetCycle_'+str(i)];p=root.get_actor_location();move(root,(x,y,p.z))
 shape('Jet_GroundMark_'+str(i),(x,y,2),(190,190,4),'Moving',kind='Cylinder',collision=False)
# 작은 언덕도 사용자 도형 규격과 두 경사의 상대 배치를 보존한다.
hill=by['R3A_Arena_HillUp'];delta=unreal.Vector(-350,-1250,hill.get_actor_location().z)-hill.get_actor_location()
for name in ['R3A_Arena_HillUp','R3A_Arena_HillDown']:by[name].set_actor_location(by[name].get_actor_location()+delta,False,False)
# Round2의 승강·왕복 이동 방식. 경로는 새 BP 템플릿에 저장해 재로딩에도 유지한다.
def mover_class(name,offset,duration):
 f=unreal.BlueprintFactory();f.set_editor_property('parent_class',unreal.StaticMeshActor);bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE,unreal.Blueprint,f)
 sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);fn=unreal.SubobjectDataBlueprintFunctionLibrary
 h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=sub.k2_gather_subobject_data_for_blueprint(bp)[0],new_class=unreal.InterpToMovementComponent,blueprint_context=bp));c=fn.get_object(fn.get_data(h));assert c,str(reason)
 c.set_editor_property('duration',duration);c.set_editor_property('behaviour_type',unreal.InterpToBehaviourType.PING_PONG);c.set_editor_property('sweep',False);c.set_editor_property('control_points',[unreal.InterpControlPoint(position_control_point=unreal.Vector(),position_is_relative=True),unreal.InterpControlPoint(position_control_point=unreal.Vector(*offset),position_is_relative=True)])
 unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp);return bp.generated_class()
lift_cls=mover_class('BP_R4_LiftMotion',(0,0,600),3);slider_cls=mover_class('BP_R4_CrossingPlatform',(1000,0,0),3)
movers=[]
for label,cls,pos in [('Lift_North',lift_cls,(-600,850,-20)),('Lift_South',lift_cls,(600,-850,-20)),('CrossingPlatform',slider_cls,(-500,0,280))]:
 a=api.spawn_actor_from_class(cls,unreal.Vector(*pos));a.set_actor_label('R4_'+label);a.set_folder_path('Round4/Platforms');c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(meshes['Cube']);c.set_material(0,mats['Moving']);c.set_editor_property('use_default_collision',False);c.set_collision_profile_name('BlockAllDynamic');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE);a.set_actor_scale3d(unreal.Vector(4,4,.4));movers.append(a)
# 기존 Round2의 개별 철망 패널을 재사용한다. 연결 입구와 두 승강기 앞뒤는 비운다.
fence_mesh=unreal.load_asset('/Game/LD_Gym/LDProps/StaticMeshes/Round2Rooftop/Meshes/SM_RT_PerimeterFence_240x360');assert fence_mesh
fence_count=0
def fence(p0,p1,height=110,parent=None):
 global fence_count
 a,b=unreal.Vector(*p0),unreal.Vector(*p1);d=b-a;count=max(1,math.ceil(d.length()/240))
 for i in range(count):
  p=a+d*((i+.5)/count);actor=api.spawn_actor_from_class(unreal.StaticMeshActor,p,unreal.MathLibrary.make_rot_from_x(d));actor.set_actor_label('R4_Fence_%03d'%fence_count);fence_count+=1;actor.set_folder_path('Round4/Fences');c=actor.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE if parent else unreal.ComponentMobility.STATIC);c.set_static_mesh(fence_mesh);c.set_editor_property('use_default_collision',False);c.set_collision_profile_name('BlockAllDynamic' if parent else 'BlockAll');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE)
  if parent:
   c.set_absolute(False,False,True);assert actor.attach_to_component(parent.root_component,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
  actor.set_actor_scale3d(unreal.Vector((d.length()/count+2)/240,1,height/360))
for row in range(3):
 z=row*360
 for a,b in [((-1780,-1780,z),(1780,-1780,z)),((1780,-1780,z),(1780,1780,z)),((1780,1780,z),(-1780,1780,z)),((-1780,1780,z),(-1780,-1780,z))]:fence(a,b,360)
for a,b in [((-1600,1600,600),(800,1600,600)),((-800,-1600,600),(1600,-1600,600)),((-1600,-1400,300),(-1600,0,300)),((1600,0,300),(1600,1400,300))]:fence(a,b)
for mover in movers[:2]:
 p=mover.get_actor_location()
 for x in [p.x-190,p.x+190]:fence((x,p.y-200,0),(x,p.y+200,0),110,mover)
# 밀리는 장애물 2개는 이동을 막지 않는 옆 공간에 둔다.
for i,(x,y) in enumerate([(-850,-500),(850,500)]):
 a=shape('Pushable_%d'%i,(x,y,49),(90,90,90),'Moving');c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_collision_profile_name('PhysicsActor');c.set_mass_override_in_kg('',24,True);c.set_linear_damping(1.1);c.set_angular_damping(1.2);c.set_use_ccd(True);c.set_simulate_physics(True)
starts=[(-500,1300,600),(500,-1300,600),(-1300,-1000,300),(1300,1000,300)]
for a,p in zip([a for a in actors if isinstance(a,unreal.PlayerStart)],starts):
 move(a,(p[0],p[1],p[2]+92));a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(a.get_actor_location(),unreal.Vector()),False)
for i,(a,p) in enumerate(zip([a for a in actors if isinstance(a,unreal.Character)],starts)):
 move(a,(p[0],p[1],p[2]+90));a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(a.get_actor_location(),unreal.Vector()),False);a.set_editor_property('auto_possess_player',unreal.AutoReceiveInput.PLAYER0 if i==0 else unreal.AutoReceiveInput.DISABLED)
for a in api.get_all_level_actors():
 old=a.get_actor_label()
 if old.startswith(('R3A_','R3L_','R3M_')):
  a.set_actor_label('R4_'+old.split('_',1)[1]);a.set_folder_path('Round4/ImportedGimmicks')
for a in api.get_all_level_actors():
 if a.get_name() in preserved:
  s=a.get_actor_scale3d();assert math.dist([s.x,s.y,s.z],preserved[a.get_name()]['scale'])<.001,a.get_actor_label()
assert hashlib.sha256(source_file.read_bytes()).hexdigest()==source_hash
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
report={'source_sha256':source_hash,'source_saved_map_only':True,'preserved':preserved,'actors':len(api.get_all_level_actors()),'fences':fence_count,'width_cm':3600,'heights_cm':[0,300,600],'gimmick_sizes_preserved':True,'pie_tested':False}
(WORK/'applied.json').write_text(json.dumps(report,indent=2));unreal.log('ROUND4_CREATED')
