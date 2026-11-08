"""완주형 L_Round3를 순환하며 싸우는 장애물 아레나 초안으로 교체한다."""
import json, shutil, math
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());MAP='/Game/Map/L_Round3';WORK=ROOT/'Saved/Round3Work'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP,'Round3를 연 상태에서 실행하세요.'
assert not any(a.get_actor_label().startswith('R3A_') for a in api.get_all_level_actors()),'이미 아레나 초안이 있습니다.'
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
backup=WORK/'L_Round3_race_before_arena.umap'
assert not backup.exists(),'이전 초안 백업을 덮어쓰지 않습니다.'
shutil.copy2(ROOT/'Content/Map/L_Round3.umap',backup)
# 수정 대상은 현재 Round3의 이전 초안 배치뿐이다.
for a in list(api.get_all_level_actors()):api.destroy_actor(a)
source=(ROOT/'Tools/Editor/build_round3_draft.py').read_text(encoding='utf-8')
exec(source[source.index('meshes={'):source.index('# 낙하 후')])
# 위의 기본 도형·이동부 생성 함수만 재사용한다.
def slab(name,x,y,w,d,z=0,color='Ground'):return floor('Arena_'+name,x,y,w,d,z,color)
def slope(name,a,b,width):
 a,b=unreal.Vector(*a),unreal.Vector(*b);v=b-a;c=(a+b)*.5
 return shape('Arena_'+name,(c.x,c.y,c.z-20),(v.length()+12,width,40),'Balance',rotation=unreal.MathLibrary.make_rot_from_x(v))

# 44m 정방형. 북서쪽 구덩이 외에는 연속된 넓은 바닥이다.
slab('WestFloor',-2000,0,400,4400)
slab('EastFloor',600,0,3200,4400)
slab('SouthFloor',-1400,-600,800,3200)
slab('NorthFloor',-1400,2000,800,400)
slab('PitBottom',-1400,1400,800,800,-180,'Balance')
slope('PitExit',(-1400,1000,-180),(-1400,1800,0),480)
moving('Bridge_Sway',(-1400,1400,-20),(220,800,40),(55,0,0),3,color='Balance')

# 중앙 교전장은 네 방향 경사로로 오르내린다. 서로 다른 길로 추격·우회한다.
slab('Center',0,0,1400,1400,120)
for i,(a,b) in enumerate([((0,-1350,0),(0,-700,120)),((0,1350,0),(0,700,120)),((-1350,0,0),(-700,0,120)),((1350,0,0),(700,0,120))]):slope('CenterRamp_%d'%i,a,b,650)
# 중앙에는 작은 엄폐물 두 개만 두어 교전 공간을 확보한다.
for x,y in [(-300,260),(320,-260)]:shape('Arena_CenterCover',(x,y,165),(220,150,90),'Ground')

# 북서: 짧은 위험 지름길과 외곽의 넓은 돌아가는 길이 공존한다.
slope('HillUp',(-1750,200,0),(-1750,600,100),500)
slope('HillDown',(-1750,600,100),(-1750,1000,0),500)
moving('Ball_Proxy',(-2000,450,100),(150,150,150),(0,-700,130),3,'Sphere')

# 북동: 국소 회전판. 주변 고정 바닥을 통해 자유롭게 빠져나올 수 있다.
turn_class=unreal.load_class(None,'/Game/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_PhysicsObstacle_TurnTable.BP_PhysicsObstacle_TurnTable_C')
turn=api.spawn_actor_from_class(turn_class,unreal.Vector(1450,1450,0));turn.set_actor_label('R3A_RotatingFloor');turn.set_folder_path('Round3Arena/Dynamic');turn.set_actor_scale3d(unreal.Vector(.8,.8,1))
for c in turn.get_components_by_class(unreal.StaticMeshComponent):
 if c.static_mesh:c.set_material(0,mats['Balance'])
pad_class=unreal.load_class(None,'/Game/LD_Gym/Interactables/BP_JumpPad.BP_JumpPad_C')
for x,y in [(0,1700),(1700,100)]:
 a=api.spawn_actor_from_class(pad_class,unreal.Vector(x,y,10));a.set_actor_label('R3A_JetPad');a.set_folder_path('Round3Arena/Dynamic')

# 남서: 왕복 추 대용 구체와 낮은 경사면. 장애물 양옆으로 탈출 가능하다.
moving('Pendulum_Proxy',(-1550,-1400,140),(200,200,200),(600,0,0),2,'Sphere')
shape('Arena_Gantry',(-1250,-1400,720),(1100,35,35),'Ground',collision=False)
for x in [-1800,-700]:shape('Arena_GantryPost',(x,-1400,360),(35,35,720),'Ground',collision=False)
slope('SouthBank',(-1650,-850,0),(-1050,-850,100),420)
slope('SouthBankReturn',(-1050,-850,100),(-650,-850,0),420)
moving('Log_Proxy',(-350,-1800,110),(650,180,180),(650,0,0),2.5,'Cube')

# 남동: 피할 공간을 남긴 국소 낙하물 두 개. 특정 길을 통과할 필요는 없다.
for i,(x,y) in enumerate([(1250,-1250),(1850,-450)]):
 moving('DropBlock_%d'%i,(x,y,820),(250,250,250),(0,0,-670),1.8+i*.4)
 slab('DropMarker_%d'%i,x,y,330,330,3,'Moving')

# 네 모서리에서 중앙을 바라보고 출발. 출발/결승선과 방향 화살표는 없다.
for i,(x,y) in enumerate([(-1900,-1900),(1900,-1900),(1900,1900),(-1900,1900)]):
 rot=unreal.MathLibrary.find_look_at_rotation(unreal.Vector(x,y,0),unreal.Vector())
 a=api.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(x,y,110),unreal.Rotator(yaw=rot.yaw));a.set_actor_label('R3A_PlayerStart_%d'%i);a.set_folder_path('Round3Arena/Starts')
 shape('Arena_SpawnMark',(x,y,2),(270,270,3),'Goal',kind='Cylinder',collision=False)
for x in [-2220,2220]:shape('Arena_Boundary',(x,0,150),(40,4480,300),'Ground')
for y in [-2220,2220]:shape('Arena_Boundary',(0,y,150),(4480,40,300),'Ground')

# 과도한 노출 보정을 없애고 고정 노출·환경광으로 검토하기 쉽게 한다.
sun=api.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,4000),unreal.Rotator(pitch=-55,yaw=-25));sun.light_component.set_editor_property('intensity',10);sun.light_component.set_editor_property('atmosphere_sun_light',True)
api.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())
sky=api.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,2000));sc=sky.light_component
cube=unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap')
if cube:
 sc.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sc.set_editor_property('cubemap',cube)
sc.set_editor_property('intensity',1.5)
pp=api.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());pp.set_editor_property('unbound',True);settings=pp.get_editor_property('settings')
for name,value in [('override_auto_exposure_bias',True),('auto_exposure_bias',0.),('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',1.),('auto_exposure_max_brightness',1.)]:settings.set_editor_property(name,value)
pp.set_editor_property('settings',settings)
p='/Game/Map/Round3Draft/Materials/M_R3_PreviewSky';m=unreal.load_asset(p)
if not m:
 m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_R3_PreviewSky','/Game/Map/Round3Draft/Materials',unreal.Material,unreal.MaterialFactoryNew());m.set_editor_property('two_sided',True);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 e=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);e.set_editor_property('constant',unreal.LinearColor(.45,.65,1.,1));unreal.MaterialEditingLibrary.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);unreal.MaterialEditingLibrary.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector());a.set_actor_label('R3A_PreviewSky');a.set_folder_path('Round3Arena/Lighting');a.set_actor_scale3d(unreal.Vector(1000,1000,1000));c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'));c.set_material(0,m);c.set_collision_profile_name('NoCollision');c.set_editor_property('cast_shadow',False)
for a in api.get_all_level_actors():
 if a.get_actor_label().startswith('R3_'):a.set_actor_label(a.get_actor_label().replace('R3_','R3A_',1));a.set_folder_path('Round3Arena/'+('Dynamic' if a.get_component_by_class(unreal.InterpToMovementComponent) else 'Geometry'))
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
count=len(api.get_all_level_actors());(WORK/'arena.json').write_text(json.dumps({'actors':count,'starts':4,'arena_cm':[4400,4400],'central_height':120,'pit_depth':180,'race':False,'pie_tested':False},indent=2))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);level.set_level_viewport_fov(55,level.get_active_viewport_config_key());p=unreal.Vector(4700,-5600,6200);unreal.EditorLevelLibrary.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(0,0,0)))
unreal.log('ROUND3_ARENA_SAVED '+str(count))
