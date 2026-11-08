"""Round3를 32m로 압축하고 양측 2층·흔들다리·굴림 통나무·주기 분수를 추가한다."""
import json, math, shutil
from datetime import datetime
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());WORK=ROOT/'Saved/Round3Layers';MAP='/Game/Map/L_Round3'
BASE='/Game/Map/Round3Draft';api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
assert not any(a.get_actor_label().startswith('R3L_') for a in api.get_all_level_actors()),'이미 2층 초안이 있습니다.'
WORK.mkdir(parents=True,exist_ok=True)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
backup=WORK/('L_Round3_before_layers_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap')
shutil.copy2(ROOT/'Content/Map/L_Round3.umap',backup)
before={a.get_actor_label():a for a in api.get_all_level_actors()}
source=(ROOT/'Tools/Editor/build_round3_draft.py').read_text(encoding='utf-8')
exec(source[source.index('meshes={'):source.index('# 낙하 후')])

def material(name,rgb,water=False):
 path=BASE+'/Materials/'+name;m=unreal.load_asset(path)
 if m:return m
 m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 e=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);e.set_editor_property('constant',unreal.LinearColor(*rgb,1))
 if water:
  m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
  lib.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  o=lib.create_material_expression(m,unreal.MaterialExpressionConstant);o.set_editor_property('r',.55);lib.connect_material_property(o,'',unreal.MaterialProperty.MP_OPACITY)
 else:lib.connect_material_property(e,'',unreal.MaterialProperty.MP_BASE_COLOR)
 lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m);return m

water_mat=material('M_R3_JetWater',(.025,.42,.85),True)
log_mat=material('M_R3_RollingLog',(.32,.14,.055))
rope_mat=material('M_R3_BridgeRope',(.12,.14,.16))

def mover_bp(name,offset,seconds):
 path=BASE+'/'+name;bp=unreal.load_asset(path)
 if not bp:
  f=unreal.BlueprintFactory();f.set_editor_property('parent_class',unreal.StaticMeshActor)
  bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE,unreal.Blueprint,f)
  sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);fn=unreal.SubobjectDataBlueprintFunctionLibrary
  h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=sub.k2_gather_subobject_data_for_blueprint(bp)[0],new_class=unreal.InterpToMovementComponent,blueprint_context=bp))
  c=fn.get_object(fn.get_data(h));assert c,str(reason)
  c.set_editor_property('duration',seconds);c.set_editor_property('behaviour_type',unreal.InterpToBehaviourType.PING_PONG);c.set_editor_property('sweep',False)
  c.set_editor_property('control_points',[unreal.InterpControlPoint(position_control_point=unreal.Vector(),position_is_relative=True),unreal.InterpControlPoint(position_control_point=unreal.Vector(*offset),position_is_relative=True)])
  unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
 return unreal.load_class(None,path+'.'+name+'_C')

bridge_cls=mover_bp('BP_R3_UpperBridgeMotion',(0,45,18),2.5)
jet_classes=[mover_bp('BP_R3_JetCycle_'+str(i),(0,0,600),seconds) for i,seconds in enumerate([2.6,3.2,3.8])]

# 기존 맵의 조명·사용자 배치 캐릭터는 유지하고 기존 전투 지형만 수평 압축한다.
factor=8/11
owned=[]
for a in api.get_all_level_actors():
 label=a.get_actor_label()
 if label.startswith('R3A_') and label!='R3A_PreviewSky':
  a.modify();p=a.get_actor_location();s=a.get_actor_scale3d()
  a.set_actor_location(unreal.Vector(p.x*factor,p.y*factor,p.z),False,False)
  a.set_actor_scale3d(unreal.Vector(s.x*factor,s.y*factor,s.z));owned.append(a)
for a in list(owned):
 if a.get_actor_label() in ['R3A_Log_Proxy','R3A_JetPad']:api.destroy_actor(a)

# 계단·경사로·2층 밑의 교전 공간과 겹치지 않도록 국소 기믹 위치를 조정한다.
positions={'R3A_RotatingFloor':(750,950,0),'R3A_Pendulum_Proxy':(-550,-800,140),'R3A_DropBlock_0':(500,-950,820),'R3A_DropBlock_1':(-500,950,820),'R3A_Arena_DropMarker_0':(500,-950,3),'R3A_Arena_DropMarker_1':(-500,950,3)}
for a in api.get_all_level_actors():
 label=a.get_actor_label()
 if label in positions:a.set_actor_location(unreal.Vector(*positions[label]),False,False)
 if label=='R3A_Arena_Gantry':a.set_actor_location(unreal.Vector(-250,-800,720),False,False)
 if label=='R3A_Arena_GantryPost':
  p=a.get_actor_location();a.set_actor_location(unreal.Vector(-800 if p.x<-700 else 300,-800,360),False,False)
 if label.startswith('R3A_PlayerStart_'):
  p=a.get_actor_location();x=650 if p.x>0 else -650;y=1100 if p.y>0 else -1100
  a.set_actor_location(unreal.Vector(x,y,110),False,False);a.set_actor_scale3d(unreal.Vector(1,1,1));a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(x,y,0),unreal.Vector()),False)
 if label=='R3A_Arena_SpawnMark':
  p=a.get_actor_location();a.set_actor_location(unreal.Vector(650 if p.x>0 else -650,1100 if p.y>0 else -1100,2),False,False)

added=[]
def part(label,pos,size,color='Ground',kind='Cube',rot=None,parent=None,collision=True,mat=None):
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),rot or unreal.Rotator());a.set_actor_label('R3L_'+label)
 a.set_folder_path('Round3Layers/'+label.split('_')[0]);c=a.static_mesh_component
 c.set_mobility(unreal.ComponentMobility.MOVABLE if parent else unreal.ComponentMobility.STATIC)
 c.set_static_mesh(meshes[kind]);c.set_material(0,mat or mats[color]);c.set_editor_property('use_default_collision',False)
 c.set_collision_profile_name(('BlockAllDynamic' if parent else 'BlockAll') if collision else 'NoCollision')
 c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE)
 if parent:
  c.set_absolute(False,False,True);assert a.attach_to_component(parent.root_component,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
 a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)));added.append(a);return a

def deck(label,x,y,w,d,z):return part(label,(x,y,z-20),(w,d,40),'Ground')
def ramp_part(label,x,y0,z0,y1,z1,w):
 d=unreal.Vector(0,y1-y0,z1-z0)
 return part(label,(x,(y0+y1)/2,(z0+z1)/2-20),(d.length()+12,w,40),'Balance',rot=unreal.MathLibrary.make_rot_from_x(d))
def rod(label,a,b,r=3,parent=None):
 a,b=unreal.Vector(*a),unreal.Vector(*b);v=b-a;p=(a+b)*.5
 return part(label,(p.x,p.y,p.z),(r*2,r*2,v.length()),kind='Cylinder',rot=unreal.MathLibrary.make_rot_from_z(v),parent=parent,collision=False,mat=rope_mat)

# 양쪽 2층은 각각 두 진입로가 있어 한 다리를 강제로 통과하지 않아도 된다.
for side,x in [('West',-1250),('East',1250)]:
 deck('Upper_'+side,x,0,700,1400,450)
 for y in [-500,500]:part('Upper_Support_'+side,(x,y,205),(100,100,410),'Ground',kind='Cylinder')
 # 바깥 난간과 안쪽 난간은 다리 입구를 비운다.
 for edge_x in [(-1580 if x<0 else 1580),(-920 if x<0 else 920)]:
  spans=[(-700,700)] if abs(edge_x)>1500 else [(-700,-220),(220,700)]
  for lo,hi in spans:
   rod('Upper_Rail',(edge_x,lo,550),(edge_x,hi,550),4)
   for y in [lo,hi]:rod('Upper_Post',(edge_x,y,450),(edge_x,y,550),4)
 for j in range(20):
  # 서쪽 북단·동쪽 남단은 높이 22.5cm, 디딤 폭 35cm의 계단이다.
  direction=1 if side=='West' else -1;y=direction*(1400-(j+.5)*35);top=(j+1)*22.5
  part('Stairs_'+side+'_%02d'%j,(x,y,top/2),(350,36,top),'Balance')
 if side=='West':ramp_part('Ramp_West',x,-1450,0,-700,450,420)
 else:ramp_part('Ramp_East',x,1450,0,700,450,420)
 deck('Upper_Landing_'+side,-900 if x<0 else 900,0,220,600,450)

# 18m의 긴 다리: 처진 개별 발판과 줄을 한 이동 루트에 부착한다.
bridge=api.spawn_actor_from_class(bridge_cls,unreal.Vector());bridge.set_actor_label('R3L_BridgeMotion');bridge.set_folder_path('Round3Layers/Bridge')
bridge.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);bridge.static_mesh_component.set_collision_profile_name('NoCollision')
def bridge_z(x):return 450-45*math.cos(math.pi*x/1800)
for j in range(12):
 x=-825+j*150;part('Bridge_Plank_%02d'%j,(x,0,bridge_z(x)-10),(148,260,20),'Balance',parent=bridge)
for y in [-150,150]:
 points=[(x,y,bridge_z(x)+105) for x in range(-900,901,150)]
 for j in range(len(points)-1):rod('Bridge_Rope',points[j],points[j+1],3,bridge)
 for x in range(-900,901,300):rod('Bridge_Post',(x,y,bridge_z(x)),(x,y,bridge_z(x)+105),3,bridge)

# 원기둥 장축을 경사면의 가로 방향으로 두어 중력으로 실제 굴러 내려가게 한다.
logs=[]
for side,x,y in [('West',-1250,-850),('East',1250,850)]:
 z=450-(abs(y)-700)*450/750+52
 a=part('RollingLog_'+side,(x,y,z),(90,90,320),kind='Cylinder',rot=unreal.Rotator(pitch=90),mat=log_mat)
 c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_collision_profile_name('PhysicsActor');c.set_mass_override_in_kg('',35,True);c.set_linear_damping(.05);c.set_angular_damping(.05);c.set_simulate_physics(True);logs.append(a)

# 물줄기는 충돌 없는 푸른 기둥, 실제 발사는 Character를 지원하는 네이티브 점프대를 이용한다.
# 점프대 자체가 바닥 아래 6m에서 솟아올라 쉬는 동안은 플레이어와 닿지 않는다.
launcher_path=BASE+'/BP_R3_JetLauncher'
launcher_bp=unreal.load_asset(launcher_path)
if not launcher_bp:
 f=unreal.BlueprintFactory();f.set_editor_property('parent_class',unreal.load_class(None,'/Script/DKUEndGame.SideScrollingJumpPad'))
 launcher_bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_R3_JetLauncher',BASE,unreal.Blueprint,f)
 unreal.BlueprintEditorLibrary.compile_blueprint(launcher_bp);unreal.EditorAssetLibrary.save_loaded_asset(launcher_bp)
pad_class=launcher_bp.generated_class();jets=[]
for i,(x,y) in enumerate([(0,-1100),(650,450),(-650,700)]):
 part('Jet_Nozzle_%d'%i,(x,y,8),(140,140,16),'Ground',kind='Cylinder')
 root=api.spawn_actor_from_class(jet_classes[i],unreal.Vector(x,y,-600));root.set_actor_label('R3L_JetCycle_%d'%i);root.set_folder_path('Round3Layers/Jets');root.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);root.static_mesh_component.set_collision_profile_name('NoCollision')
 column=part('Jet_Water_%d'%i,(x,y,-300),(85,85,600),kind='Cylinder',parent=root,collision=False,mat=water_mat)
 column.static_mesh_component.set_editor_property('cast_shadow',False)
 pad=api.spawn_actor_from_class(pad_class,unreal.Vector(x,y,-600));pad.set_actor_label('R3L_JetLauncher_%d'%i);pad.set_folder_path('Round3Layers/Jets');pad.modify()
 pad.set_editor_property('z_strength',900.)
 pad.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector(100,100,100),True)
 pad.get_component_by_class(unreal.BoxComponent).set_relative_location(unreal.Vector(),False,False)
 assert pad.attach_to_component(root.root_component,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
 pad.root_component.set_editor_property('relative_location',unreal.Vector())
 for c in pad.get_components_by_class(unreal.StaticMeshComponent):c.set_visibility(False);c.set_hidden_in_game(True);c.set_collision_profile_name('NoCollision')
 jets.append(root)

assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
speed=json.loads((WORK/'before.json').read_text())['character']['speed']
report={'backup':str(backup),'width_before_cm':4400,'width_after_cm':3200,'walk_speed_cm_s':speed,'straight_crossing_before_s':4400/speed,'straight_crossing_after_s':3200/speed,'ground_event_spacing_cm':[550,750],'ground_event_spacing_s':[550/speed,750/speed],'upper_height_cm':450,'upper_bridge_length_cm':1800,'upper_bridge_width_cm':260,'bridge_walk_s':1800/speed,'jet_periods_s':[5.2,6.4,7.6],'actors':len(api.get_all_level_actors()),'pie_tested':False}
(WORK/'applied.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);level.set_level_viewport_fov(55,level.get_active_viewport_config_key());p=unreal.Vector(4100,-4700,4300);unreal.EditorLevelLibrary.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(0,0,200)))
unreal.log('ROUND3_LAYERS_SAVED '+str(report['actors']))
