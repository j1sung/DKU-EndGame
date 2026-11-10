"""열린 L_Round2_Organic의 사용자 배치를 유지하며 옥상 테마를 적용한다."""
import hashlib
import json
import math
import shutil
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir())
WORK=ROOT/'Saved/RooftopWork'
SRC=WORK/'Sources'
MAP='/Game/Map/L_Round2_Organic'
BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round2Rooftop'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
assert not any(k.startswith('RT_') for k in actors), '이미 테마가 적용된 맵입니다.'
assert not unreal.EditorAssetLibrary.does_directory_exist(BASE), '전용 에셋 덮어쓰기 금지'
manifest=json.loads((SRC/'manifest.json').read_text(encoding='utf-8'))
before=json.loads((WORK/'before.json').read_text(encoding='utf-8'))
assert len(actors)==len(before['actors'])
for row in before['actors']:
    p=actors[row['label']].get_actor_location()
    assert math.dist([p.x,p.y,p.z],row['loc'])<.01, '조사 이후 배치 변경 감지'
# 최신 사용자 편집을 먼저 저장하고, 테마 적용 직전 원본을 Saved에 보관한다.
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
shutil.copy2(ROOT/'Content/Map/L_Round2_Organic.umap',WORK/'L_Round2_Organic_before_rooftop.umap')
original_round2_hash=hashlib.sha256((ROOT/'Content/Map/L_Round2.umap').read_bytes()).hexdigest()
tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
mats={}


def scalar(m,value,x,y):
    e=lib.create_material_expression(m,unreal.MaterialExpressionConstant,x,y)
    e.set_editor_property('r',value)
    return e


for key,color in manifest['palette'].items():
    m=tools.create_asset('M_RT_'+key,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    base=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-650,0)
    base.set_editor_property('constant',unreal.LinearColor(*color,1))
    noise=lib.create_material_expression(m,unreal.MaterialExpressionNoise,-650,180)
    noise.set_editor_property('scale',.014 if key in ['Roof','Plaster'] else .045)
    noise.set_editor_property('levels',2);noise.set_editor_property('quality',1)
    multiply=lib.create_material_expression(m,unreal.MaterialExpressionMultiply,-400,150)
    multiply.set_editor_property('const_b',.02 if key in ['Plaster','Coping','Roof','Stain'] else .01)
    lib.connect_material_expressions(noise,'',multiply,'A')
    add=lib.create_material_expression(m,unreal.MaterialExpressionAdd,-200,0)
    lib.connect_material_expressions(base,'',add,'A');lib.connect_material_expressions(multiply,'',add,'B')
    lib.connect_material_property(add,'',unreal.MaterialProperty.MP_BASE_COLOR)
    metallic=.65 if key in ['Steel','Zinc'] else 0
    lib.connect_material_property(scalar(m,metallic,-200,330),'',unreal.MaterialProperty.MP_METALLIC)
    lib.connect_material_property(scalar(m,.32 if key=='Glass' else .5 if metallic else .88,-200,420),'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(m);mats[key]=m

tasks=[]
for item in manifest['meshes']:
    t=unreal.AssetImportTask();t.filename=str(SRC/(item['name']+'.obj'))
    t.destination_path=BASE+'/Meshes';t.destination_name=item['name'];t.automated=True;t.save=False
    opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False
    opt.static_mesh_import_data.combine_meshes=True;t.options=opt;tasks.append(t)
tools.import_asset_tasks(tasks)
meshsys=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
meshes={}
for item in manifest['meshes']:
    mesh=unreal.load_asset(BASE+'/Meshes/'+item['name']);assert mesh,item['name']
    for i,slot in enumerate(mesh.static_materials):
        key=str(slot.material_slot_name);assert key in mats,key;mesh.set_material(i,mats[key])
    meshsys.remove_collisions(mesh)
    if item['collision']:
        meshsys.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    meshes[item['name']]=mesh
for asset in unreal.EditorAssetLibrary.list_assets(BASE,True,False):
    assert unreal.EditorAssetLibrary.save_asset(asset,only_if_is_dirty=False)

added=[];changed_fences=[]


def paint(a,key):
    a.modify()
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        c.modify()
        for i in range(c.get_num_materials()):c.set_material(i,mats[key])


def spawn(mesh,loc,yaw=0,folder='Equipment',scale=(1,1,1),collision=False):
    a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(yaw=yaw))
    a.set_actor_label('RT_'+mesh.replace('SM_RT_','')+'_%03d'%len(added));a.set_folder_path('Round2Rooftop/'+folder)
    a.set_actor_scale3d(unreal.Vector(*scale));c=a.static_mesh_component
    c.set_static_mesh(meshes[mesh]);c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION)
    if collision:
        c.set_editor_property('use_default_collision',False);c.set_collision_profile_name('BlockAll')
        c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE)
    else:
        c.set_editor_property('use_default_collision',False);c.set_collision_profile_name('NoCollision')
    added.append(a);return a


with unreal.ScopedEditorTransaction('Theme Round2 as interconnected urban rooftops'):
    for label,a in actors.items():
        if label.startswith('SM_Dummy_Block_'):paint(a,'Plaster')
        elif label.startswith(('SM_Dummy_Wall','SM_Dummy_Window','SM_Dummy_Stairs','SM_Dummy_Rubble_Plank','SM_Dummy_Arch')):paint(a,'Steel')
        elif label=='Floor':paint(a,'Seam')
        elif label.startswith('SM_Dummy_GroundMound'):paint(a,'Roof')
        elif label.startswith('SM_Dummy_Cylinder'):paint(a,'Zinc')
        elif label.startswith(('OR_EntryStripe','OR_SweepArea','OR_SliderTrack')):paint(a,'Orange' if 'Entry' in label else 'Steel')
        elif label.startswith('OR_PushBlock'):paint(a,'Orange')
        elif label.startswith('OR_PushBall'):paint(a,'Teal')
        elif label=='BP_PhysicsObstacle_MovingPlatform':paint(a,'Zinc')
        elif label in ['OR_SlidingBlock','OR_Sweeper_00','OR_Sweeper_01']:paint(a,'Orange')
        if not label.startswith('OR_FencePanel_'):continue
        a.modify();c=a.static_mesh_component;c.modify()
        group=label.split('_')[2]
        if group=='Lift':
            paint(a,'Steel');continue
        if group=='Stairs':
            old=c.static_mesh.get_name()
            item=next(item for item in manifest['meshes'] if item.get('old_stair')==old)
            name=item['name']
        elif group=='Hub':name='SM_RT_Rail_240'
        else:
            # 연속된 외곽 구간 대부분은 파라펫, 설비 옆의 일부 구간만 혼합형 난간.
            segment=int(label.split('_')[3]);name='SM_RT_Hybrid_240' if segment%5==1 else 'SM_RT_Parapet_240'
        c.set_static_mesh(meshes[name])
        for i in range(c.get_num_materials()):c.set_material(i,None)
        changed_fences.append(label)
    for item in manifest['placements']:spawn(item['mesh'],item['loc'],folder=item['folder'])
    # 얇은 방수 마감 위에서도 원래 기믹 범위·진입 표식이 보이도록 시각 복사본만 올린다.
    for label in ['OR_SweepArea_00','OR_SweepArea_01','OR_SliderTrack','OR_EntryStripe_00','OR_EntryStripe_01']:
        source=actors[label]
        marker=api.spawn_actor_from_class(unreal.StaticMeshActor,source.get_actor_location()+unreal.Vector(0,0,8),source.get_actor_rotation())
        marker.set_actor_label('RT_SurfaceMarker_'+label);marker.set_folder_path('Round2Rooftop/Markers')
        marker.set_actor_scale3d(source.get_actor_scale3d());marker.static_mesh_component.set_static_mesh(source.static_mesh_component.static_mesh)
        marker.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        marker.static_mesh_component.set_editor_property('use_default_collision',False);marker.static_mesh_component.set_collision_profile_name('NoCollision')
        paint(marker,'Orange' if 'Entry' in label else 'Steel');added.append(marker)
    equipment=[
        ('HVAC_Double',(-1120,-2290,965.167),90),('ExhaustFan',(180,-1900,965.167),0),
        ('HVAC_Double',(1730,-2450,907.607),0),('WaterTank',(1935,-1645,907.607),0),
        ('HVAC_Double',(-2770,-650,2535.215),90),('WaterTank',(-2780,1840,2535.215),0),
        ('AccessHut',(-1650,1850,2535.215),90),('DuctStraight',(-2570,2080,2535.215),90),
        ('HVAC_Double',(-1010,2280,2750.5),0),('AccessHut',(180,2210,2750.5),0),
        ('HVAC_Double',(1320,1970,2279.9),90),('DuctStraight',(1460,1430,2279.9),0),
        ('ExhaustFan',(1300,-900,2279.9),0),
        ('HVAC_Single',(-2770,120,2535.215),0),('HVAC_Single',(2070,-2240,907.607),0),
    ]
    for mesh,pos,yaw in equipment:spawn('SM_RT_'+mesh,pos,yaw,collision=True)
    for pos,yaw in [((-2370,2080,2535.215),90),((1460,1630,2279.9),0)]:
        spawn('SM_RT_DuctElbow',pos,yaw)
    for x,y,z,yaw in [(-1340,-1820,965,0),(-2925,1450,2535,90),(1425,-630,2280,90),(-1250,2450,2750,180)]:
        spawn('SM_RT_UtilityPipes',(x,y,z),yaw)
    for x,y,z,yaw in [(-1310,-2190,970,0),(-2880,-650,2540,0),(-930,2410,2755,90),(1440,1840,2285,0)]:
        spawn('SM_RT_CableTray',(x,y,z),yaw)
    for x,y,z in [(-1300,-2600,968),(2020,-2580,910),(-2880,2050,2538),(280,2430,2753),(1430,2150,2283)]:
        spawn('SM_RT_Drain',(x,y,z))
    # 옥상 아래 벽면의 실외기: 보행면 위에 돌출하지 않는다.
    for x,y,z,yaw in [(-320,-2725,450,0),(1050,-2720,530,0),(1600,780,1710,90),(-3040,300,1900,-90),(-900,2545,2080,180)]:
        spawn('SM_RT_WallAC',(x,y,z),yaw,folder='Facades')
    # 중앙 허브의 기존 두 경사로·구멍·상단 연결판은 그대로 두고 하부 거더만 덧댄다.
    cube=unreal.load_asset('/Engine/BasicShapes/Cube')
    for label in ['SM_Dummy_Window_4x4','SM_Dummy_Wall_4x5','SM_Dummy_Wall_4x6','SM_Dummy_Wall_4x7']:
        a=actors[label];o,e=a.get_actor_bounds(False)
        for x in [o.x-e.x+12,o.x+e.x-12]:
            support=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,o.y,o.z-e.z-22),unreal.Rotator())
            support.set_actor_label('RT_BridgeGirder_%03d'%len(added));support.set_folder_path('Round2Rooftop/Bridges')
            support.static_mesh_component.set_static_mesh(cube);support.set_actor_scale3d(unreal.Vector(.15,e.y*2/100,.38))
            support.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);paint(support,'Steel');added.append(support)
            support.static_mesh_component.set_editor_property('use_default_collision',False);support.static_mesh_component.set_collision_profile_name('NoCollision')
    # 원래 맵 밖의 도시 실루엣은 장식이며 플레이어 충돌에 관여하지 않는다.
    skyline=[(-4500,-3000,0),(-4500,-1000,1),(-4550,1200,2),(-4300,3400,0),
             (-2300,4300,1),(0,4400,2),(2400,4250,0),(3700,2700,1),
             (3950,500,2),(3900,-1700,0),(3500,-3900,1),(1200,-4350,2),
             (-1200,-4400,0),(-3200,-4600,2)]
    for i,(x,y,variant) in enumerate(skyline):
        spawn('SM_RT_BackgroundBuilding_%02d'%variant,(x,y,-1300-(i%3)*170),90*(i%2),folder='SurroundingCity')
    api.set_selected_level_actors([])
    # 높이·형태·시작 지점 및 기존 기믹 변환을 모두 보존한다.
    for row in before['actors']:
        a=actors[row['label']];p,r,s=a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()
        assert math.dist([p.x,p.y,p.z],row['loc'])<.01,row['label']
        assert math.dist([r.pitch,r.yaw,r.roll],row['rot'])<.01,row['label']
        assert math.dist([s.x,s.y,s.z],row['scale'])<.001,row['label']
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
assert original_round2_hash==hashlib.sha256((ROOT/'Content/Map/L_Round2.umap').read_bytes()).hexdigest()
report={'map':MAP,'added':len(added),'meshes':len(meshes),'materials':len(mats),'fences_replaced':len(changed_fences),
        'preserved_actor_transforms':len(before['actors']),'actors':len(api.get_all_level_actors()),'pie_tested':False}
(WORK/'applied.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ROUND2_ROOFTOP_APPLIED '+json.dumps(report))
