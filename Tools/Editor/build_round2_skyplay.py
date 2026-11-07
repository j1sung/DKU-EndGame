"""L_Round2에서 만든 별도 SkyPlay 복제본에 4단 입체 캐주얼 아레나를 구성한다."""
import unreal,math,json,hashlib
from pathlib import Path
MAP='/Game/Map/L_Round2_SkyPlay'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
assert 'SP_Hub' not in actors,'이미 구성된 새 맵입니다.'
root=Path(unreal.Paths.project_dir());original=root/'Content/Map/L_Round2.umap';source_hash=hashlib.sha256(original.read_bytes()).hexdigest()
mats={k:unreal.load_asset('/Game/LD_Gym/LDProps/StaticMeshes/Playground/Materials/M_Playground_'+k) for k in ['Mint','Teal','Coral','Yellow','Cream','Blue','Dark','Lavender']};assert all(mats.values())
meshes={k:unreal.load_asset('/Engine/BasicShapes/'+k) for k in ['Cube','Cylinder','Sphere']}
created=[];ramps=[];pads=[];lifts=[];spawns=[]
def paint(a,color):
 a.modify()
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  c.modify()
  for i in range(c.get_num_materials()):c.set_material(i,mats[color])
def box_transform(a,loc,size,rot=unreal.Rotator()):
 a.modify();a.root_component.modify();a.set_actor_location(unreal.Vector(*loc),False,False);a.set_actor_rotation(rot,False);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
def shape(label,mesh,loc,size,color,collision=False,rot=unreal.Rotator(),folder='Details'):
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),rot);a.set_actor_label('SP_'+label);a.set_folder_path('SkyPlay/'+folder);c=a.static_mesh_component;c.set_static_mesh(meshes[mesh]);c.set_material(0,mats[color]);c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)));created.append(a);return a
def slab(label,x,y,top,sx,sy,color,thickness=40,folder='Routes'):
 return shape(label,'Cube',(x,y,top-thickness/2),(sx,sy,thickness),color,True,folder=folder)
def rod(label,a,b,diameter,color,folder='Details'):
 v=unreal.Vector(*b)-unreal.Vector(*a);p=(unreal.Vector(*a)+unreal.Vector(*b))*.5
 return shape(label,'Cylinder',(p.x,p.y,p.z),(diameter,diameter,v.length()),color,False,unreal.MathLibrary.make_rot_from_z(v),folder)
def ramp(label,a,b,width=420,color='Cream'):
 dx,dy,dz=[b[i]-a[i] for i in range(3)];run=math.hypot(dx,dy);length=math.sqrt(run*run+dz*dz);pitch=math.degrees(math.atan2(dz,run));yaw=math.degrees(math.atan2(dy,dx));thick=32
 assert abs(pitch)<40,(label,pitch)
 # 경사로 상면의 양 끝 높이를 설계값에 맞추고 두께는 아래쪽에 둔다.
 loc=((a[0]+b[0])/2,(a[1]+b[1])/2,(a[2]+b[2])/2-thick/(2*math.cos(math.radians(pitch))))
 rot=unreal.Rotator(pitch=pitch,yaw=yaw,roll=0)
 deck=shape(label,'Cube',loc,(length+12,width,thick),color,True,rot,folder='Routes')
 side=(-dy/run,dx/run)
 for sign in [-1,1]:
  offset=sign*(width/2-9);p=(loc[0]+side[0]*offset,loc[1]+side[1]*offset,loc[2]+22)
  shape(label+'_Edge','Cube',p,(length+12,12,12),'Yellow',False,rot,folder='RouteTrim')
 for t in [.15,.4,.65,.9]:
  p=(a[0]+dx*t,a[1]+dy*t,a[2]+dz*t+3)
  shape(label+'_Tread','Cube',p,(14,width-50,3),'Teal',False,rot,folder='RouteTrim')
 ramps.append({'label':deck.get_actor_label(),'from':a,'to':b,'width':width,'slope_degrees':pitch})
 return deck
# 원형의 네 구역·캐릭터를 재사용하고 나머지 그레이박스는 새 맵 안에서만 아래로 옮긴다.
main=['SM_Dummy_Block_100x103','SM_Dummy_Block_100x102','SM_Dummy_Block_100x101','SM_Dummy_Block_100x100']
keep=set(main+['Floor','BP_JumpPad','SP_Lift_LowToHub'])
for label,a in actors.items():
 if label in keep or label.startswith('BP_Chicken') or a.get_class().get_name() in ['DirectionalLight','SkyAtmosphere','SkyLight','ExponentialHeightFog','VolumetricCloud']:continue
 if label=='SM_SkySphere':continue
 a.modify();p=a.get_actor_location();a.set_actor_location(unreal.Vector(p.x,p.y,-20000),False,False);a.set_actor_hidden_in_game(True);a.set_actor_enable_collision(False);a.set_folder_path('SkyPlay/ArchivedGraybox')
# 원본 80m 바닥을 회수 바닥으로 사용해 낙하 후에도 복귀할 수 있게 한다.
actors['Floor'].modify();actors['Floor'].set_actor_location(unreal.Vector(0,0,-120),False,False);paint(actors['Floor'],'Mint');actors['Floor'].set_folder_path('SkyPlay/Recovery')
blocks=[(-1150,-1150,180,'Teal','01'),(-1150,1150,480,'Coral','02'),(1150,1150,780,'Blue','03'),(1150,-1150,1080,'Lavender','04')]
for i,(x,y,top,color,number) in enumerate(blocks):
 a=actors[main[i]];a.modify();a.static_mesh_component.set_static_mesh(meshes['Cube']);box_transform(a,(x,y,(top-120)/2),(1700,1700,top+120));paint(a,color);a.set_folder_path('SkyPlay/FourBlocks')
 slab('IslandTop_'+number,x,y,top+2,1698,1698,'Cream',4,'FourBlocks')
 # 넓은 색 띠·기둥·둥근 캡으로 네 구역의 높이와 외곽을 읽기 쉽게 한다.
 for side in [-1,1]:
  shape('IslandBand_'+number,'Cube',(x,y+side*852,top-55),(1700,14,80),color,False,folder='BlockTrim')
  shape('IslandBand_'+number,'Cube',(x+side*852,y,top-55),(14,1700,80),color,False,folder='BlockTrim')
 for cx in [-820,820]:
  for cy in [-820,820]:
   shape('CornerPost_'+number,'Cylinder',(x+cx,y+cy,top+50),(26,26,100),color,False,folder='BlockTrim');shape('CornerCap_'+number,'Sphere',(x+cx,y+cy,top+105),(40,40,40),'Yellow',False,folder='BlockTrim')
 # 낮은 장애물은 중앙 필수 통로를 비워 두고 측면에 놓는다.
 for dx,dy in [(-390,200),(390,-200)]:
  shape('Bumper_'+number,'Cylinder',(x+dx,y+dy,top+36),(170,170,68),color,True,folder='PlayObstacles');shape('BumperCap_'+number,'Sphere',(x+dx,y+dy,top+72),(175,175,24),'Yellow',False,folder='PlayObstacles')
 # 외곽 출발 포털과 안전 바깥 난간. 중앙 진입은 막지 않는다.
 sx=x+(-420 if x<0 else 420);sy=y+(-420 if y<0 else 420)
 shape('SpawnRing_'+number,'Cylinder',(sx,sy,top+4),(330,330,6),color,False,folder='Starts');shape('SpawnInset_'+number,'Cylinder',(sx,sy,top+8),(270,270,4),'Cream',False,folder='Starts')
 for off in [-210,210]:rod('StartGate_'+number,(sx+off,sy,top),(sx+off,sy,top+290),24,color,'Starts')
 rod('StartGateTop_'+number,(sx-210,sy,top+290),(sx+210,sy,top+290),30,color,'Starts')
 for t in [-1,1]:
  ox=x+(-850 if x<0 else 850);oy=y+(-850 if y<0 else 850)
  rod('OuterRail_'+number,(ox,y+t*420-220,top+90),(ox,y+t*420+220,top+90),14,'Teal','SafetyRails')
  rod('OuterRail_'+number,(x+t*420-220,oy,top+90),(x+t*420+220,oy,top+90),14,'Teal','SafetyRails')
 # 네 개 PlayerStart와 기존 테스트 캐릭터의 몸 크기를 맞춘다.
 start=api.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(sx,sy,top+90));start.set_actor_label('SP_PlayerStart_'+number);start.set_editor_property('player_start_tag',unreal.Name('SkyPlay_'+number));start.set_folder_path('SkyPlay/Starts')
 pawn=actors['BP_ChickenCharacter'+('' if i==0 else str(i+1))];pawn.modify();pawn.set_actor_location(unreal.Vector(sx,sy,top+90),False,True);pawn.set_actor_rotation(unreal.Rotator(pitch=0,yaw=math.degrees(math.atan2(-sy,-sx)),roll=0),True);pawn.set_editor_property('auto_possess_player',unreal.AutoReceiveInput.PLAYER0 if i==0 else unreal.AutoReceiveInput.DISABLED);pawn.set_folder_path('SkyPlay/Starts')
 spawns.append({'number':number,'surface':top,'location':[sx,sy,top+90]})
# 300cm씩 올라가는 주 순환로. 길 폭 420cm, 경사 약 26.6도.
ramp('Loop_A_B',(-1150,-310,180),(-1150,310,480),420)
ramp('Loop_B_C',(-310,1150,480),(310,1150,780),420)
ramp('Loop_C_D',(1150,310,780),(1150,-310,1080),420)
# 900cm를 한 번에 뛰지 않고 남쪽을 따라 세 번에 나누어 돌아온다.
slab('SouthHighLanding',1150,-2210,1080,500,450,'Lavender')
slab('SouthHighEntry',1150,-2050,1080,420,300,'Cream')
ramp('South_D_780',(1150,-2300,1080),(250,-2300,780),420)
slab('SouthMidLanding',100,-2300,780,400,500,'Blue')
ramp('South_780_480',(100,-2300,780),(-800,-2300,480),420)
slab('SouthLowLanding',-1050,-2300,480,500,500,'Coral')
ramp('South_480_A',(-1150,-2300,480),(-1150,-1400,180),420)
# 중앙 4.8m 합류 구역과 높이가 다른 평행 진입로.
shape('Hub','Cylinder',(0,0,455),(720,720,50),'Teal',True,folder='Hub');shape('HubTop','Cylinder',(0,0,482),(700,700,4),'Yellow',False,folder='Hub');shape('HubInset','Cylinder',(0,0,485),(600,600,3),'Cream',False,folder='Hub')
for x,color in [(-150,'Coral'),(150,'Blue')]:slab('NorthLanding',x,1150,480 if x<0 else 780,320,440,color)
slab('NorthLowLane',-150,650,480,260,1040,'Cream')
ramp('NorthHighLane',(150,1150,780),(150,190,480),260)
slab('SouthUpperLanding',150,-1150,1080,320,440,'Lavender')
ramp('SouthHighLane',(150,-1150,1080),(150,-190,480),260)
slab('SouthLiftExit',-150,-570,480,260,760,'Cream')
# 기존 상하 발판을 그대로 사용: 가로만 확대하고 Z 배율은 1로 두어 300cm 이동을 유지한다.
lift_cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_PhysicsObstacle_MovingPlatform')
for label,x,y,z in [('SP_Lift_LowToHub',-150,-1150,160),('SP_Lift_Recovery',-2200,-1150,-140)]:
 a=actors.get(label) or api.spawn_actor_from_class(lift_cls,unreal.Vector(x,y,z));a.modify();a.set_actor_label(label);a.set_actor_location(unreal.Vector(x,y,z),False,False);a.set_actor_scale3d(unreal.Vector(1.8,1.8,1));a.set_folder_path('SkyPlay/Lifts');paint(a,'Teal');lifts.append({'label':label,'bottom_surface':z+20,'top_surface':z+320,'width':360})
 slab(label+'_Bottom',x,y-310,z+20,400,260,'Yellow',30)
slab('RecoveryTopExit',-2020,-1150,180,240,420,'Cream')
# 회수 바닥에서 최저 구역으로 올라오는 항상 열려 있는 보행 경사로.
ramp('RecoveryRamp',(-2800,-1900,-120),(-2000,-1900,180),500)
# 각 단계의 점프대: 높이차 300cm, 위쪽 큰 착지면을 향한 빠른 경로.
jump_cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/LD_Gym/LevelPrototyping/Interactable/JumpPad/BP_JumpPad')
for i,(loc,vel,landing) in enumerate([((-800,-700,183),(0,950,1000),(-800,900,480)),((-700,1550,483),(950,0,1000),(900,1550,780)),((1550,700,783),(0,-950,1000),(1550,-900,1080))]):
 a=actors['BP_JumpPad'] if i==0 else api.spawn_actor_from_class(jump_cls,unreal.Vector(*loc));a.modify();a.set_actor_label('SP_JumpPad_%02d'%i);a.set_actor_location(unreal.Vector(*loc),False,False);a.set_actor_scale3d(unreal.Vector(1.2,1.2,1));a.set_editor_property('Velocity',unreal.Vector(*vel));a.set_folder_path('SkyPlay/JumpPads')
 shape('JumpLaunchRing','Cylinder',(loc[0],loc[1],loc[2]-1),(310,310,4),'Yellow',False,folder='JumpPadTrim')
 shape('JumpLandingRing','Cylinder',(landing[0],landing[1],landing[2]+5),(460,460,5),'Yellow',False,folder='JumpPadTrim')
 shape('JumpLandingInset','Cylinder',(landing[0],landing[1],landing[2]+8),(390,390,3),'Cream',False,folder='JumpPadTrim')
 pads.append({'label':a.get_actor_label(),'velocity':vel,'start':loc,'landing':landing})
# 바닥의 둥근 회수 징검 표식과 지지 기둥, 구역별 패널로 입체감을 더한다.
for x,y in [(-2400,-500),(-2400,300),(2400,700),(2400,-300),(-500,2500),(400,2500)]:
 shape('RecoveryIsland','Cylinder',(x,y,-75),(520,520,90),'Teal',True,folder='Recovery');shape('RecoveryIslandTop','Cylinder',(x,y,-28),(510,510,4),'Cream',False,folder='Recovery')
for x,y,top in [(-150,650,480),(150,650,640),(150,-700,800),(0,0,440),(100,-2300,780),(-1050,-2300,480)]:
 shape('BridgeSupport','Cylinder',(x,y,(top-120)/2),(55,55,top+120),'Teal',False,folder='Supports')
for i,(x,y,top,color,num) in enumerate(blocks):
 for k in range(4):
  z=-60+k*(top+60)/4
  shape('SideStripe_'+num,'Cube',(x+853,y,z),(12,1550,12),'Cream',False,folder='BlockTrim')
# 회전판 등 다른 기능은 가져오지 않고 기존 조명·세계 설정은 복제본에 유지한다.
api.set_selected_level_actors([])
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-4400,-6000,5400),unreal.Rotator(pitch=-32,yaw=54,roll=0))
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
assert hashlib.sha256(original.read_bytes()).hexdigest()==source_hash,'원본 파일 변경 감지'
report={'map':MAP,'original_sha256':source_hash,'blocks':blocks,'ramps':ramps,'spawns':spawns,'jump_pads':pads,'lifts':lifts,'capsule_radius':34,'capsule_half_height':88,'walk_speed':275,'walkable_floor_angle':44.765,'min_route_width':260,'actors':len(api.get_all_level_actors()),'pie_tested':False}
(root/'Saved/Round2Work/layout.json').write_text(json.dumps(report,indent=2))
unreal.log('SKYPLAY_LAYOUT_SAVED')
