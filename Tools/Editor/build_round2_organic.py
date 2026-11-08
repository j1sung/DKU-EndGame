"""L_Round2의 배치·형태를 유지한 별도 원형 맵에 펜스와 이동 기믹을 추가한다."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
SOURCE = '/Game/Map/L_Round2'
MAP = '/Game/Map/L_Round2_Organic'
BASE = '/Game/LD_Gym/LDProps/StaticMeshes/Round2Organic'
OUT = ROOT/'Saved/Round2OrganicWork'
OUT.mkdir(parents=True,exist_ok=True)
api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world()
source_hash = hashlib.sha256((ROOT/'Content/Map/L_Round2.umap').read_bytes()).hexdigest()
if world.get_path_name().split('.')[0] == SOURCE:
    assert not unreal.EditorAssetLibrary.does_asset_exist(MAP), '기존 완성 맵을 덮어쓰지 않습니다.'
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
    world = editor.get_editor_world()
assert world.get_path_name().split('.')[0] == MAP
actors = {a.get_actor_label():a for a in api.get_all_level_actors()}
assert 'OR_Fence_Outer' not in actors, '이미 구성된 맵에서는 실행하지 않습니다.'

def snapshot(a):
    p,s,r = a.get_actor_location(),a.get_actor_scale3d(),a.get_actor_rotation()
    return {'loc':[p.x,p.y,p.z],'scale':[s.x,s.y,s.z],'rot':[r.pitch,r.yaw,r.roll],
            'meshes':[c.static_mesh.get_path_name() for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]}

original = {k:snapshot(a) for k,a in actors.items() if not k.startswith('OR_')}
triangles = []
for label,a in actors.items():
    if label.startswith(('OR_','BP_Chicken')) or label in ['Floor','SM_SkySphere']:
        continue
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if not c.static_mesh:
            continue
        tx,mesh = c.get_world_transform(),c.static_mesh
        for section in range(mesh.get_num_sections(0)):
            verts,ids,normals,uv,tan = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,section)
            verts = [unreal.MathLibrary.transform_location(tx,v) for v in verts]
            normals = [unreal.MathLibrary.transform_direction(tx,n) for n in normals]
            for i in range(0,len(ids),3):
                indices = ids[i:i+3]
                if sum(normals[j].z for j in indices)/3 >= .65:
                    triangles.append({'actor':label,'p':[[verts[j].x,verts[j].y,verts[j].z] for j in indices]})
spec = importlib.util.spec_from_file_location('round2_organic_geometry',ROOT/'Tools/Editor/round2_organic_geometry.py')
geometry = importlib.util.module_from_spec(spec)
spec.loader.exec_module(geometry)
plan = geometry.make_sources(triangles,OUT/'MeshSources')
tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for name in plan['meshes']:
    assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Meshes/'+name), '생성 에셋이 이미 존재합니다: '+name
    task = unreal.AssetImportTask()
    task.filename = str(OUT/'MeshSources'/(name+'.obj'))
    task.destination_path,task.destination_name = BASE+'/Meshes',name
    task.automated,task.save = True,False
    options = unreal.FbxImportUI()
    options.import_materials,options.import_textures = False,False
    options.static_mesh_import_data.combine_meshes = True
    task.options = options
    tasks.append(task)
tools.import_asset_tasks(tasks)
materials = {}
for name,color in [('Guard',(.18,.21,.24)),('Movable',(.12,.39,.47)),('Hazard',(.9,.32,.06)),('Marker',(.55,.57,.59))]:
    path = BASE+'/Materials/M_Round2_'+name
    assert not unreal.EditorAssetLibrary.does_asset_exist(path)
    material = tools.create_asset('M_Round2_'+name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    rgb = unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
    rgb.set_editor_property('constant',unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant)
    roughness.set_editor_property('r',.8)
    unreal.MaterialEditingLibrary.connect_material_property(roughness,'',unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    materials[name] = material
mesh_api = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
meshes = {}
for name in plan['meshes']:
    mesh = unreal.load_asset(BASE+'/Meshes/'+name)
    assert mesh
    mesh_api.remove_collisions(mesh)
    moving = 'SweepBar' in name or 'Slider' in name
    if moving:
        mesh_api.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',
        unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if moving else unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    mesh.set_material(0,materials['Hazard'] if moving else materials['Guard'])
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    meshes[name] = mesh
created = []

def shape(label,mesh,location,scale=(1,1,1),material=None,collision=False,folder='Markers',rotation=None):
    a = api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*location),rotation or unreal.Rotator())
    a.set_actor_label('OR_'+label)
    a.set_folder_path('Round2Organic/'+folder)
    c = a.static_mesh_component
    c.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(*scale))
    if material:
        c.set_material(0,materials[material])
    c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION)
    created.append(a)
    return a

cube = unreal.load_asset('/Engine/BasicShapes/Cube')
cylinder = unreal.load_asset('/Engine/BasicShapes/Cylinder')
for group in ['Outer','Hub','Stairs','Lift']:
    shape('Fence_'+group,meshes['SM_Round2_OpenFence_'+group],(0,0,0),folder='OpenFences')
guards = []
def guard(label,a,b,height):
    dx,dy = b[0]-a[0],b[1]-a[1]
    length = math.hypot(dx,dy)
    center = [(a[k]+b[k])/2 for k in range(3)]
    center[2] += height/2
    actor = shape(label,cube,center,((length+8)/100,.22,(height+abs(b[2]-a[2]))/100),
                  collision=True,folder='FenceCollision',rotation=unreal.Rotator(yaw=math.degrees(math.atan2(dy,dx))))
    c = actor.static_mesh_component
    c.modify()
    c.set_editor_property('use_default_collision',False)
    c.set_collision_profile_name('Custom')
    c.set_visibility(False,True)
    c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN,unreal.CollisionResponseType.ECR_BLOCK)
    c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PHYSICS_BODY,unreal.CollisionResponseType.ECR_BLOCK)
    guards.append({'label':actor.get_actor_label(),'a':a,'b':b,'height':height})

for i,s in enumerate(plan['segments']):
    guard('EdgeGuard_%03d'%i,s['a'],s['b'],plan['collision_height'])
for i,(a,b) in enumerate([((-2810,-2430,965),(-1555,-2430,965)),
                         ((-2810,-2430,965),(-2810,-1210,965)),
                         ((-1555,-2430,965),(-1555,-1210,965))]):
    guard('LiftGuard_%02d'%i,a,b,1955)

def paint(actor,kind):
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        c.modify()
        if c.static_mesh:
            for i in range(c.get_num_materials()):
                c.set_material(i,materials[kind])

def marker(label,x,y,z,diameter):
    shape(label,cylinder,(x,y,z+1.5),(diameter/100,diameter/100,.03),'Marker')

# 기본 이동로의 가운데를 비우고 넓은 구역에 우회 가능한 회전봉을 배치한다.
sweep_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_PhysicsObstacle_TurnTable')
sweepers = []
for i,(x,y,z,rate) in enumerate([(-500,-1750,965.1673,28),(-2250,850,2535.2148,-24)]):
    a = actors.get('OR_Candidate_BP_PhysicsObstacle_TurnTable') if i == 0 else None
    a = a or api.spawn_actor_from_class(sweep_class,unreal.Vector(x,y,z+60))
    a.modify()
    a.set_actor_label('OR_Sweeper_%02d'%i)
    a.set_actor_location(unreal.Vector(x,y,z+60),False,False)
    a.set_folder_path('Round2Organic/Sweepers')
    component = next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'Cylinder_0')
    component.modify()
    component.set_static_mesh(meshes['SM_Round2_SweepBar'])
    component.set_collision_profile_name('BlockAllDynamic')
    move = a.get_component_by_class(unreal.RotatingMovementComponent)
    move.modify()
    move.set_editor_property('rotation_rate',unreal.Rotator(yaw=rate))
    paint(a,'Hazard')
    marker('SweepArea_%02d'%i,x,y,z,920)
    sweepers.append({'label':a.get_actor_label(),'position':[x,y,z+60],'yaw_rate':rate})

# 원본 승강판은 보존한다. 별도 인스턴스의 제어점만 수평 왕복으로 바꾼다.
lift_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_PhysicsObstacle_MovingPlatform')
slider = api.spawn_actor_from_class(lift_class,unreal.Vector(1050,800,2279.90004))
slider.set_actor_label('OR_SlidingBlock')
slider.set_folder_path('Round2Organic/Slider')
platform = next(c for c in slider.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'Platform')
platform.modify()
platform.set_static_mesh(meshes['SM_Round2_Slider'])
movement = slider.get_component_by_class(unreal.InterpToMovementComponent)
movement.modify()
points = [unreal.InterpControlPoint(position_control_point=unreal.Vector(0,0,0),position_is_relative=True),
          unreal.InterpControlPoint(position_control_point=unreal.Vector(0,550,0),position_is_relative=True)]
movement.set_editor_property('control_points',points)
movement.set_editor_property('duration',3.5)
paint(slider,'Hazard')
shape('SliderTrack',cube,(1050,1075,2281),(2.9,8.5,.025),'Marker')

# 기존 범퍼는 선택 경로에 둬 밟지 않고 지나갈 수 있게 한다.
bumper = actors.get('OR_Candidate_BP_SimpleObstacle_Bumper')
if not bumper:
    cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_SimpleObstacle_Bumper')
    bumper = api.spawn_actor_from_class(cls,unreal.Vector(-2150,1700,2535.2148))
bumper.modify()
bumper.set_actor_label('OR_OptionalBumper')
bumper.set_actor_location(unreal.Vector(-2150,1700,2535.2148),False,False)
bumper.set_editor_property('Duration',1.8)
bumper.set_folder_path('Round2Organic/Bumper')
paint(bumper,'Hazard')

# 질량과 감쇠를 둔 블록·공은 서로 밀거나 돌아서 지나가며 길 모양을 바꾼다.
pushables = []
sphere = unreal.load_asset('/Engine/BasicShapes/Sphere')
for i,(x,y,z,ball) in enumerate([(-950,-650,965.1673,False),(-450,-2300,965.1673,True),
                                (1650,-1900,907.6073,False),(-2700,-2450,965.6073,True),
                                (-1800,-500,2535.2148,False),(-900,2000,2750.5000,True),
                                (0,1300,2750.5000,False),(1250,-500,2279.9000,True)]):
    size = 120 if ball else 100
    a = shape('PushBall_%02d'%i if ball else 'PushBlock_%02d'%i,sphere if ball else cube,
              (x,y,z+size/2+4),(size/100,)*3,'Movable',True,'Pushables')
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('use_default_collision',False)
    c.set_collision_profile_name('PhysicsActor')
    c.set_mass_override_in_kg('',18 if ball else 24,True)
    c.set_linear_damping(.75 if ball else 1.1)
    c.set_angular_damping(1.2)
    c.set_use_ccd(True)
    c.set_simulate_physics(True)
    a.set_replicates(True)
    a.set_replicate_movement(True)
    pushables.append({'label':a.get_actor_label(),'location':[x,y,z+size/2+4],'ball':ball,'mass_kg':18 if ball else 24})

# 계단과 승강기 방향 표시는 장식이며 원본 이동 장치를 건드리지 않는다.
for i,(x,y,z) in enumerate([(-2183,-1130,965.6073),(1849,-1250,907.6073)]):
    shape('EntryStripe_%02d'%i,cube,(x,y,z+1),(7, .35,.025),'Marker')
api.set_selected_level_actors([])
assert all(snapshot(a) == original[k] for k,a in actors.items() if k in original), '원본 액터 변환 또는 메시 변경 감지'
assert hashlib.sha256((ROOT/'Content/Map/L_Round2.umap').read_bytes()).hexdigest() == source_hash
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
report = {'map':MAP,'source_hash':source_hash,'original_actors':original,'fences':plan,
          'guards':guards,'sweepers':sweepers,'pushables':pushables,'slider':{'from':[1050,800,2279.90004],'travel':[0,550,0],'duration':3.5},
          'actor_count':len(api.get_all_level_actors()),'pie_tested':False}
(OUT/'applied.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ROUND2_ORGANIC_SAVED '+str(report['actor_count']))
