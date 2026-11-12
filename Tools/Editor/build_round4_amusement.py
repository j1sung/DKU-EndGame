"""기존 Round4를 보존하고 별도의 놀이동산 시소 레벨을 최초 생성한다."""
import hashlib
import json
import math
import runpy
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());WORK=ROOT/'Saved/Round4Amusement';SRC=WORK/'Sources'
BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round4';MAP='/Game/Map/L_Round4_Amusement'
assert not unreal.EditorAssetLibrary.does_asset_exist(MAP),'기존 새 레벨은 자동으로 덮어쓰지 않습니다.'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(),'다른 미저장 에셋을 먼저 보존해야 합니다.'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(),'다른 미저장 맵을 먼저 보존해야 합니다.'
other_hash={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/Map').glob('L_Round*.umap')}
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
mesh_api=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
manifest=json.loads((SRC/'manifest.json').read_text(encoding='utf-8'))
mats={};meshes={}


def constant(m,v):
    e=lib.create_material_expression(m,unreal.MaterialExpressionConstant);e.set_editor_property('r',v);return e


for key,rgb in manifest['palette'].items():
    m=unreal.load_asset(BASE+'/Materials/M_R4A_'+key)
    if not m:
        m=tools.create_asset('M_R4A_'+key,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        e=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);e.set_editor_property('constant',unreal.LinearColor(*rgb,1))
        lib.connect_material_property(e,'',unreal.MaterialProperty.MP_BASE_COLOR)
        rough=.3 if key in ['Chrome','Glass'] else .52 if key in ['Teal','Coral','Yellow'] else .8
        lib.connect_material_property(constant(m,rough),'',unreal.MaterialProperty.MP_ROUGHNESS)
        lib.connect_material_property(constant(m,.8 if key=='Chrome' else .15 if key in ['Teal','Coral','Steel'] else 0),'',unreal.MaterialProperty.MP_METALLIC)
        if key=='Bulb':lib.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if key=='Deck':
            runpy.run_path(str(ROOT/'Tools/Editor/polish_round4_deck_surface.py'))['configure_material'](m)
        elif key=='Paving':
            p=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
            grain=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
            grain.set_editor_property('code','float2 q=abs(frac(P.xy/100)-.5); float seam=step(.49,max(q.x,q.y)); return float3(%s)*(1-seam*.12);'%','.join(map(str,rgb)))
            inp=unreal.CustomInput();inp.set_editor_property('input_name','P');grain.set_editor_property('inputs',[inp])
            grain.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
            lib.connect_material_expressions(p,'',grain,'P');lib.connect_material_property(grain,'',unreal.MaterialProperty.MP_BASE_COLOR)
        lib.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_NANITE)
        lib.recompile_material(m)
    mats[key]=m

tasks=[]
for row in manifest['meshes']:
    if unreal.EditorAssetLibrary.does_asset_exist(BASE+'/'+row['name']):continue
    t=unreal.AssetImportTask();t.filename=str(SRC/(row['name']+'.obj'));t.destination_path=BASE;t.destination_name=row['name'];t.automated=True;t.save=False
    opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;t.options=opt;tasks.append(t)
tools.import_asset_tasks(tasks)
for row in manifest['meshes']:
    mesh=unreal.load_asset(BASE+'/'+row['name']);assert mesh,row['name']
    for i,slot in enumerate(mesh.static_materials):mesh.set_material(i,mats[str(slot.material_slot_name)])
    mesh_api.remove_collisions(mesh)
    if row['collision'] in ['box','cylinder']:
        mesh_api.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX if row['collision']=='box' else unreal.ScriptCollisionShapeType.NDOP26)
    elif row['collision']=='complex':
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if row['nanite']:
        ns=mesh_api.get_nanite_settings(mesh);ns.set_editor_property('enabled',True);mesh_api.set_nanite_settings(mesh,ns,True)
    meshes[row['name']]=mesh

rig_class=unreal.load_class(None,'/Script/DKUEndGame.AmusementSeesawRig');assert rig_class
bp=unreal.load_asset(BASE+'/Blueprints/BP_R4A_SeesawRig')
if not bp:
    f=unreal.BlueprintFactory();f.set_editor_property('parent_class',rig_class)
    bp=tools.create_asset('BP_R4A_SeesawRig',BASE+'/Blueprints',unreal.Blueprint,f)
cdo=unreal.get_default_object(bp.generated_class())
for key,name in [('deck_mesh','FightingDeck_1800x1200'),('rod_mesh','PusherRod_100'),('head_mesh','PusherHead'),('bridge_mesh','BoardingBridge_200'),('gate_mesh','SlidingGate')]:cdo.set_editor_property(key,meshes['SM_R4A_'+name])
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
for asset in list(mats.values())+list(meshes.values())+[bp]:assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.new_level(MAP)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
added=[]


def spawn(name,p,scale=(1,1,1),yaw=0,folder='Structure',collision=False,shadow=True):
    a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*p),unreal.Rotator(yaw=yaw));a.set_actor_label('R4A_'+name+'_%03d'%len(added));a.set_folder_path('Round4Amusement/'+folder)
    c=a.static_mesh_component;c.set_static_mesh(meshes['SM_R4A_'+name]);c.set_editor_property('use_default_collision',False)
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE);c.set_cast_shadow(shadow)
    a.set_actor_scale3d(unreal.Vector(*scale));added.append(a);return a


def box(key,p,size,folder='Structure',collision=True):return spawn('Block_'+key,p,tuple(x/100 for x in size),folder=folder,collision=collision)


def fences(p0,p1,folder='Fence',max_step=230):
    x0,y0,z=p0;x1,y1,_=p1;length=math.hypot(x1-x0,y1-y0);n=math.ceil(length/max_step)
    for i in range(n):
        t=(i+.5)/n;spawn('FencePanel_240',(x0+(x1-x0)*t,y0+(y1-y0)*t,z),(length/n/230,1,1),math.degrees(math.atan2(y1-y0,x1-x0)),folder,True)


def sign(text,p):
    spawn('SignBoard',p,folder='Entrance')
    a=api.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(p[0],p[1]-9,p[2]),unreal.Rotator(yaw=-90))
    a.set_actor_label('R4A_Sign_'+text);a.set_folder_path('Round4Amusement/Entrance')
    c=a.get_component_by_class(unreal.TextRenderComponent);c.set_text(text);c.set_world_size(36);c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)


rig=api.spawn_actor_from_class(bp.generated_class(),unreal.Vector());rig.set_actor_label('R4A_SeesawRig');rig.set_folder_path('Round4Amusement/Ride')
spawn('PivotPedestal',(0,0,0),collision=True)
for side in [-1,1]:spawn('PusherHousing',(side*480,0,0),collision=True)
box('Cream',(0,0,-25),(1500,650,50))
box('Paving',(0,-350,-35),(4200,3500,70),'Ground')
for side in [-1,1]:
    # 고정 발판은 최대 회전 시소의 X 외접 범위 901cm보다 바깥이다.
    box('Cream',(side*1230,0,487.5),(260,240,975),'Boarding')
    spawn('StairFlight_875',(side*1230,-1370,0),(1,1,975/875),folder='Boarding',collision=True)
    fences((side*1370,-120,975),(side*1370,120,975),'Boarding')
    fences((side*1090,120,975),(side*1370,120,975),'Boarding')
    # 위 발판의 시소 쪽 입구는 게이트가 열리는 180cm 폭만 비운다.
    for y in [-350,-850,-1250]:spawn('ArrowMark',(side*1230,y,975*(y+1370)/1250+1),(1,1,1),90 if side<0 else -90,'Entrance',shadow=False)
spawn('OperatorBooth',(-850,-1120,0),yaw=180,folder='Entrance',collision=True)
# 짧은 U 대기줄. 하단 오른쪽 진행 -> 오른쪽 회전 -> 상단 왼쪽 -> 계단.
fences((-1350,-1770,0),(-680,-1770,0),'Queue')
fences((-1100,-1530,0),(-850,-1530,0),'Queue')
fences((-1350,-1650,0),(-850,-1650,0),'Queue')
fences((-680,-1770,0),(-680,-1530,0),'Queue')
fences((-850,-1530,0),(-680,-1530,0),'Queue')
# 끝쪽 회전 폭 170cm를 확보하며 난간을 가로지르지 않는 선.
for x in [-1230,-1000,-800]:spawn('ArrowMark',(x,-1710,.4),(.5,.5,1),0,'Queue',shadow=False)
for x in [-800,-1000,-1230]:spawn('ArrowMark',(x,-1590,.4),(.5,.5,1),180,'Queue',shadow=False)
fences((-1350,-1530,0),(-1350,-1370,0),'Queue')
sign('ENTRY',(-1430,-1840,145));sign('EXIT',(1230,-1740,145))
# 동선이 밖으로 이어져 보이되 실제 경기 중에는 숨은 외곽 볼륨이 출입구도 막는다.
for p0,p1 in [((-1720,-1900,0),(1720,-1900,0)),((-1720,-1900,0),(-1720,1000,0)),((1720,-1900,0),(1720,1000,0)),((-1720,1000,0),(1720,1000,0))]:fences(p0,p1)
barriers=[]
for p0,p1 in [((-1720,-1900),(1720,-1900)),((-1720,1000),(1720,1000)),((-1720,-1900),(-1720,1000)),((1720,-1900),(1720,1000))]:
    x0,y0=p0;x1,y1=p1;length=math.hypot(x1-x0,y1-y0);count=math.ceil(length/300)
    for i in range(count):
        t=(i+.5)/count;a=api.spawn_actor_from_class(unreal.BlockingVolume,unreal.Vector(x0+(x1-x0)*t,y0+(y1-y0)*t,1550))
        a.set_actor_label('R4A_InvisibleBoundary_%02d'%len(barriers));a.set_folder_path('Round4Amusement/InvisibleBoundary')
        _,e=a.get_actor_bounds(False);size=(length/count+12,45,2900) if x1!=x0 else (45,length/count+12,2900)
        a.set_actor_scale3d(unreal.Vector(size[0]/(2*e.x),size[1]/(2*e.y),size[2]/(2*e.z)))
        c=a.get_component_by_class(unreal.BrushComponent);c.set_collision_profile_name('Custom');c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
        c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN,unreal.CollisionResponseType.ECR_BLOCK);c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        c.set_cast_shadow(False);c.set_editor_property('generate_overlap_events',False);a.set_actor_hidden_in_game(True);barriers.append(a)
# 놀이동산 배경은 경기 울타리 바로 바깥에 큰 실루엣으로 모은다.
spawn('FerrisFrame_4000',(-550,2000,0),folder='Background')
for i in range(16):
    t=i*math.tau/16;spawn('FerrisCabin',(-550+2000*math.cos(t),2000,2250+2000*math.sin(t)-300),folder='Background')
spawn('CoasterBackdrop',(-500,3800,0),folder='Background')
spawn('DropTower',(2500,2400,0),folder='Background')
for x,y,yaw in [(-2200,-1100,90),(-2200,0,90),(-2200,950,90),(2200,-1000,-90),(2200,100,-90),(1600,1600,0)]:spawn('ConcessionBooth',(x,y,0),yaw=yaw,folder='Background')
for x,y in [(-1500,-2250),(1500,-2250),(-2150,500),(2100,700),(-1200,1280),(1000,1300)]:spawn('Planter',(x,y,0),folder='Background',collision=False)
box('Paving',(0,1300,-50),(10000,11000,40),'Background',False)
# 전용 바닥/기둥/난간 등은 모두 Round4 폴더의 메시만 참조한다.
sun=api.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,2000),unreal.Rotator(pitch=-48,yaw=-40));sun.set_actor_label('R4A_Sun');sun.set_folder_path('Round4Amusement/Lighting');sun.light_component.set_editor_property('intensity',7.)
api.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector()).set_actor_label('R4A_SkyAtmosphere')
sky=api.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,2000));sky.set_actor_label('R4A_SkyLight');sky.set_folder_path('Round4Amusement/Lighting')
sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.light_component.set_editor_property('intensity',1.25)
pp=api.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());pp.set_actor_label('R4A_Exposure');pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings')
for key,value in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',1.),('auto_exposure_max_brightness',1.)]:settings.set_editor_property(key,value)
pp.set_editor_property('settings',settings)
world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Game/Testing/Knockdown/BP_CF_KnockdownTestGameMode.BP_CF_KnockdownTestGameMode_C'))
chicken=unreal.load_class(None,'/Game/Characters/BP_ChickenCharacter.BP_ChickenCharacter_C');assert chicken
for i,(x,y) in enumerate([(-500,-280),(500,-280),(-500,280),(500,280)]):
    start=api.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(x,y,1070),unreal.Rotator(yaw=0 if x<0 else 180));start.set_actor_label('R4A_PlayerStart_'+str(i+1));start.set_editor_property('tags',['ArenaSpawn']);start.set_folder_path('Round4Amusement/Players')
    a=api.spawn_actor_from_class(chicken,unreal.Vector(x,y,1070),unreal.Rotator(yaw=0 if x<0 else 180));a.set_actor_label('R4A_PreviewPlayer_'+str(i+1));a.set_folder_path('Round4Amusement/Players')
    a.set_editor_property('auto_possess_player',unreal.AutoReceiveInput.PLAYER0 if i==0 else unreal.AutoReceiveInput.DISABLED);a.set_editor_property('auto_possess_ai',unreal.AutoPossessAI.DISABLED)
    a.character_movement.set_editor_property('run_physics_with_no_controller',True)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
for name,h in other_hash.items():assert hashlib.sha256((ROOT/'Content/Map'/name).read_bytes()).hexdigest()==h,name
pose=unreal.Vector(4000,-5400,3800);unreal.EditorLevelLibrary.set_level_viewport_camera_info(pose,unreal.MathLibrary.find_look_at_rotation(pose,unreal.Vector(0,-100,600)))
level.set_level_viewport_fov(60,level.get_active_viewport_config_key());level.editor_set_game_view(True)
(WORK/'created.json').write_text(json.dumps(dict(map=MAP,actors=len(api.get_all_level_actors()),meshes=len(meshes),materials=len(mats),barriers=len(barriers),other_maps_unchanged=other_hash,deck_cm=[1800,1200],pivot_z=850),indent=2),encoding='utf-8')
unreal.log('ROUND4_AMUSEMENT_CREATED '+MAP)
