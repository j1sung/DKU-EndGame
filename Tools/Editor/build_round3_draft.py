"""기본 도형과 기존 기믹으로 L_Round3 초안을 최초 1회 생성한다."""
import json
from pathlib import Path
import unreal

MAP='/Game/Map/L_Round3'
ROOT=Path(unreal.Paths.project_dir())
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.EditorAssetLibrary.does_asset_exist(MAP),'기존 초안을 덮어쓰지 않습니다.'
assert level.new_level(MAP)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Game/Testing/Knockdown/BP_CF_KnockdownTestGameMode.BP_CF_KnockdownTestGameMode_C'))
meshes={n:unreal.load_asset('/Engine/BasicShapes/'+n) for n in ['Cube','Sphere','Cylinder']}
mats={};lib=unreal.MaterialEditingLibrary
for name,rgb in {'Ground':(.31,.34,.36),'Balance':(.12,.32,.46),'Moving':(.72,.27,.055),'Goal':(.13,.48,.29),'Recovery':(.16,.18,.2)}.items():
 path='/Game/Map/Round3Draft/Materials/M_R3_'+name
 m=unreal.load_asset(path)
 if not m:
  m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_R3_'+name,'/Game/Map/Round3Draft/Materials',unreal.Material,unreal.MaterialFactoryNew())
  e=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);e.set_editor_property('constant',unreal.LinearColor(*rgb,1));lib.connect_material_property(e,'',unreal.MaterialProperty.MP_BASE_COLOR)
  lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
 mats[name]=m

def shape(label,pos,size,color='Ground',kind='Cube',rotation=None,collision=True):
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),rotation or unreal.Rotator());a.set_actor_label('R3_'+label);a.set_folder_path('Round3Draft/'+label.split('_')[0])
 c=a.static_mesh_component;c.set_static_mesh(meshes[kind]);c.set_material(0,mats[color]);a.set_actor_scale3d(unreal.Vector(*(s/100 for s in size)))
 c.set_editor_property('use_default_collision',False);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE)
 return a

def floor(label,x,y,w,d,z=0,color='Ground'):return shape(label,(x,y,z-20),(w,d,40),color)

def ramp(label,x,y1,z1,y2,z2,width,color='Balance'):
 v=unreal.Vector(0,y2-y1,z2-z1);rot=unreal.MathLibrary.make_rot_from_x(v)
 return shape(label,(x,(y1+y2)/2,(z1+z2)/2-20),(v.length()+10,width,40),color,rotation=rot)

def sign(label,text,x,y,z=240):
 a=api.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(x,y,z),unreal.Rotator(yaw=-90));a.set_actor_label('R3_Label_'+label);a.set_folder_path('Round3Draft/Labels');c=a.text_render;c.set_text(text);c.set_world_size(85);c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)

def moving(label,pos,size,delta,duration=3,kind='Cube',rotation=None,color='Moving'):
 # 재로딩 시 SCS 인스턴스 배열이 초기화되지 않게 이동 경로는 전용 BP 기본값에 둔다.
 name='BP_R3_'+label;path='/Game/Map/Round3Draft/'+name
 cls=unreal.load_class(None,path+'.'+name+'_C');assert cls,path
 a=api.spawn_actor_from_class(cls,unreal.Vector(*pos));a.set_actor_label('R3_'+label);a.set_folder_path('Round3Draft/Dynamic')
 if kind=='Cylinder':kind='Cube';size=(650,180,180)
 c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(meshes[kind]);c.set_material(0,mats[color]);c.set_collision_profile_name('BlockAllDynamic');a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
 return a

# 낙하 후 시작 부근의 바깥 경사로로 복귀하는 회수 바닥.
floor('Recovery_Catch',0,5100,5200,12500,-350,'Recovery')
for x in [-2250,2250]:ramp('Recovery_Return',x,-900,-350,500,0,400,'Recovery')
floor('Recovery_ReturnLanding',0,500,5200,400)
floor('Start',0,0,2200,1000)
for i,x in enumerate([-600,-200,200,600]):
 a=api.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(x,-240,110),unreal.Rotator(yaw=90));a.set_actor_label('R3_PlayerStart_%d'%i)
sign('Start','START  /  +Y',0,-450)

# 얕은 구덩이와 낮은 언덕. 구덩이에는 복귀 경사로를 둔다.
floor('Terrain_BeforePit',0,900,2200,800)
floor('Terrain_AfterPit',0,2100,2200,800)
floor('Terrain_LeftOfPit',-975,1500,250,400)
floor('Terrain_RightOfPit',375,1500,1450,400)
floor('Terrain_PitBottom',-600,1500,500,400,-180,'Balance')
ramp('Terrain_PitExit',-600,1300,-180,1700,0,480)
ramp('Terrain_HillUp',550,1700,0,2150,160,650)
ramp('Terrain_HillDown',550,2150,160,2600,0,650)
ramp('Terrain_RollRise',-250,2300,0,2650,140,500)
ramp('Terrain_Descent',-250,2650,140,3400,0,500)
floor('Terrain_End',0,2950,2200,900)
moving('Ball_Proxy',(-250,3200,90),(150,150,150),(0,-700,130),3,'Sphere')
sign('Terrain','01  PITS / HILLS',0,1000,350)
floor('Fork_Wait',0,3650,3600,500)

# 왼쪽은 좁은 흔들다리와 왕복 구체, 오른쪽은 넓은 우회로와 통나무.
moving('Bridge_Sway',(-850,5000,-20),(220,2200,40),(55,0,0),3,color='Balance')
moving('Pendulum_Proxy',(-1150,5050,140),(200,200,200),(600,0,0),2,'Sphere')
shape('Pendulum_Gantry',(-850,5050,750),(1100,40,40),'Ground',collision=False)
for x in [-1400,-300]:shape('Pendulum_Post',(x,5050,375),(40,40,750),'Ground',collision=False)
floor('Detour_Entry',900,4000,1800,300)
floor('Detour_Long',1650,5000,400,2200)
floor('Detour_Exit',900,6000,1800,300)
moving('Log_Proxy',(1325,5000,110),(180,180,650),(650,0,0),2.5,'Cylinder',unreal.Rotator(pitch=90))
sign('Fork','02  BRIDGE  /  DETOUR',0,3700,500)
floor('Rejoin',0,6450,3600,700)
sign('Rejoin','REST / REJOIN',0,6500)

# 회전 원판과 고정 우회 발판. 분수는 기존 점프대로 대체한다.
for x in [-900,900]:floor('Rotor_Bypass',x,7400,350,1200)
floor('Rotor_Entry',0,6900,700,200)
floor('Rotor_Exit',0,7900,700,200)
turn_class=unreal.load_class(None,'/Game/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_PhysicsObstacle_TurnTable.BP_PhysicsObstacle_TurnTable_C')
turn=api.spawn_actor_from_class(turn_class,unreal.Vector(0,7400,0));turn.set_actor_label('R3_RotatingFloor');turn.set_folder_path('Round3Draft/Dynamic')
c=next(c for c in turn.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh);c.set_material(0,mats['Balance']);c.set_editor_property('relative_scale3d',unreal.Vector(11,11,.3));turn.get_component_by_class(unreal.RotatingMovementComponent).set_editor_property('rotation_rate',unreal.Rotator(yaw=22))
pad_class=unreal.load_class(None,'/Game/LD_Gym/Interactables/BP_JumpPad.BP_JumpPad_C')
for x in [-900,900]:
 a=api.spawn_actor_from_class(pad_class,unreal.Vector(x,7400,10));a.set_actor_label('R3_JetJumpPad_'+str(x));a.set_folder_path('Round3Draft/Dynamic')
sign('Rotor','03  ROTOR / JET PADS',0,7400,550)
floor('Final_Wait',0,8250,2200,500)
ramp('Final_Slope',0,8500,0,10000,400,2200)
for i,(x,y) in enumerate([(-450,9000),(450,9550)]):
 z=(y-8500)/1500*400
 moving('DropBlock_%d'%i,(x,y,z+800),(250,250,250),(0,0,-670),1.8+i*.4)
 floor('DropZone_%d'%i,x,y,330,330,z+25,'Moving')
sign('Final','04  FALLING BLOCKS',0,8750,600)
floor('Goal',0,10500,2200,1000,400,'Goal')
sign('Goal','FINISH / DRAFT',0,10500,700)
# 회수 바닥 외곽만 막고 좁은 다리에서는 회수 바닥으로 떨어지게 한다.
for x in [-2600,2600]:shape('Boundary',(x,5100,150),(50,12500,1000),'Recovery')
for y in [-1150,11350]:shape('Boundary',(0,y,150),(5200,50,1000),'Recovery')
sun=api.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,5000),unreal.Rotator(pitch=-50,yaw=-35));sun.light_component.set_editor_property('intensity',5);sun.light_component.set_editor_property('atmosphere_sun_light',True)
api.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())
sky=api.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,1000));sky.light_component.set_editor_property('real_time_capture',True)
pp=api.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());pp.set_actor_label('R3_PreviewExposure');pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings');settings.set_editor_property('override_auto_exposure_bias',True);settings.set_editor_property('auto_exposure_bias',-2);pp.set_editor_property('settings',settings)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
count=len(api.get_all_level_actors());out=ROOT/'Saved/Round3Work';out.mkdir(exist_ok=True)
(out/'created.json').write_text(json.dumps({'map':MAP,'actors':count,'starts':4,'draft_proxies':['ball translation','sway bridge','lateral pendulum ball','lateral log','jump-pad jets','vertical drop blocks'],'pie_tested':False},indent=2))
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(9000,-6000,13000),unreal.MathLibrary.find_look_at_rotation(unreal.Vector(9000,-6000,13000),unreal.Vector(0,5000,0)))
unreal.log('ROUND3_DRAFT_SAVED '+str(count))
