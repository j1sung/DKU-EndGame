"""L_LobbyJS를 보존하고 별도 놀이터 로비를 생성한다. Unreal Editor Python에서 실행."""
import unreal
import math
import json
from pathlib import Path

SOURCE = '/Game/Map/L_LobbyJS'
TARGET = '/Game/Map/L_LobbyJS_Playground'
ASSET_DIR = '/Game/Map/Playground/Materials'
actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0] == SOURCE, '원본 로비를 연 상태에서 실행하세요.'
assert not unreal.EditorAssetLibrary.does_asset_exist(TARGET), '기존 놀이터 맵을 덮어쓰지 않습니다.'
original = actors_api.get_all_level_actors()
original_classes = {a.get_actor_label(): a.get_class().get_path_name() for a in original}
assert unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET), '새 맵 저장 실패'
unreal.EditorLoadingAndSavingUtils.load_map(TARGET)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0] == TARGET, '새 맵 전환 확인 실패'

palette = {
 'Mint': (0.15,0.46,0.37), 'Teal': (0.025,0.25,0.30),
 'Coral': (0.92,0.24,0.13), 'Yellow': (1.0,0.65,0.08),
 'Cream': (0.89,0.85,0.68), 'Blue': (0.08,0.32,0.64),
 'Dark': (0.025,0.055,0.065), 'Lavender': (0.43,0.28,0.64)
}
materials = {}
lib = unreal.MaterialEditingLibrary
for name, color in palette.items():
    path = ASSET_DIR + '/M_Playground_' + name
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), '생성 머티리얼 경로가 이미 존재합니다: '+path
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_Playground_'+name, ASSET_DIR, unreal.Material, unreal.MaterialFactoryNew())
    base = lib.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -450, 0)
    base.set_editor_property('constant', unreal.LinearColor(*color, 1))
    noise = lib.create_material_expression(m, unreal.MaterialExpressionNoise, -650, 220)
    noise.set_editor_property('scale', 1.8)
    noise.set_editor_property('levels', 1)
    noise.set_editor_property('quality', 1)
    amount = lib.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 230)
    amount.set_editor_property('const_b', 0.035)
    lib.connect_material_expressions(noise, '', amount, 'A')
    add = lib.create_material_expression(m, unreal.MaterialExpressionAdd, -180, 30)
    lib.connect_material_expressions(base, '', add, 'A')
    lib.connect_material_expressions(amount, '', add, 'B')
    lib.connect_material_property(add, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(m, unreal.MaterialExpressionConstant, -180, 400)
    rough.set_editor_property('r', 0.86)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    materials[name] = m

by_label = {a.get_actor_label(): a for a in actors_api.get_all_level_actors()}
def paint(actor, color):
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh:
            for i in range(c.get_num_materials()): c.set_material(i, materials[color])

# 원본 메시·Blueprint를 수정하지 않고 새 맵의 인스턴스에만 적용한다.
for label, a in by_label.items():
    if label == 'Floor': paint(a, 'Mint')
    elif label == 'Landscape': a.set_editor_property('landscape_material', materials['Mint'])
    elif 'GroundMound' in label: paint(a, 'Teal')
    elif 'Arch' in label: paint(a, 'Coral')
    elif 'House' in label: paint(a, 'Yellow')
    elif 'Block' in label: paint(a, 'Lavender')
    elif 'Drum' in label: paint(a, 'Coral' if label.endswith('01') else 'Blue')
    elif label in ['SM_Dummy_Wall_4x4','SM_Dummy_Wall_4x5','SM_Dummy_Wall_4x6','SM_Dummy_Wall_4x7']:
        paint(a, 'Mint')
        # 외벽 높이는 원본 로비를 유지한다.
    elif label.startswith('SM_Dummy_Window_'): paint(a, 'Teal')
    elif label in ['SM_Dummy_Wall_4x9','SM_Dummy_Wall_4x10','SM_Dummy_Wall_4x11','SM_Dummy_Wall_4x12']: paint(a, 'Cream')
    elif label=='SM_Dummy_Wall_4x8': paint(a, 'Teal')
    elif 'MovingPlatform' in label: paint(a, 'Blue')
    elif 'TurnTable' in label: paint(a, 'Coral')
    elif 'JumpPad' in label:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            if c.static_mesh: c.set_material(0, materials['Dark' if c.get_name()=='Cylinder' else 'Yellow'])

meshes = {n:unreal.load_asset('/Engine/BasicShapes/'+n) for n in ['Cube','Cylinder','Sphere']}
created=[]
def shape(label, mesh, loc, size, color, rot=None, parent=None, collision=False):
    a=actors_api.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), rot or unreal.Rotator())
    a.set_actor_label('PG_'+label)
    a.set_folder_path('Playground/'+('RideDetails' if parent else 'Environment'))
    c=a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE if parent else unreal.ComponentMobility.STATIC)
    c.set_static_mesh(meshes[mesh]); c.set_material(0,materials[color])
    c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION)
    a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
    if parent:
        a.attach_to_component(parent, '', unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    created.append(a)
    return a

def rod(label, start, end, diameter=12, color='Cream', parent=None):
    v=unreal.Vector(*end)-unreal.Vector(*start)
    center=(unreal.Vector(*start)+unreal.Vector(*end))*0.5
    rotation=unreal.MathLibrary.make_rot_from_z(v)
    return shape(label,'Cylinder',(center.x,center.y,center.z),(diameter,diameter,v.length()),color,rotation,parent)

# 회전하는 실제 판의 메시 컴포넌트에 붙여 손잡이도 함께 회전시킨다.
turn=by_label['BP_PhysicsObstacle_TurnTable']
turn_mesh=next(c for c in turn.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh)
for i in range(6):
    angle=i*math.tau/6
    x=518+285*math.cos(angle); y=306+285*math.sin(angle)
    rod('Roundabout_Post_%02d'%i,(x,y,10),(x,y,110),14,'Yellow',turn_mesh)
    ex=518+135*math.cos(angle); ey=306+135*math.sin(angle)
    rod('Roundabout_Grip_%02d'%i,(x,y,110),(ex,ey,110),14,'Cream',turn_mesh)
    shape('Roundabout_Seat_%02d'%i,'Sphere',(x,y,40),(110,80,22),'Yellow',parent=turn_mesh)
rod('Roundabout_Hub',(518,306,10),(518,306,115),24,'Teal',turn_mesh)

# 점프대 주위의 패딩: 트리거와 원래 충돌은 그대로 둔다.
for i in range(12):
    a=i*math.tau/12
    shape('Trampoline_Pad_%02d'%i,'Sphere',(668+155*math.cos(a),-1349+155*math.sin(a),208),(65,65,24),'Yellow')

# 2층의 열린 놀이 데크. 기존 진입부와 미끄럼틀 접속부는 열어 둔다.
for x in [810,1340]:
    for y in [-1920,-850]:
        rod('Tower_Post',(x,y,0),(x,y,800),28,'Coral')
        shape('Tower_Cap','Sphere',(x,y,804),(44,44,44),'Cream')
for x in [810,1340]:
    rod('Tower_Rail',(x,-1920,770),(x,-850,770),16,'Cream')
    for y in range(-1850,-850,150):
        rod('Tower_Baluster',(x,y,675),(x,y,770),9,'Teal')
rod('Tower_BackRail',(810,-1920,770),(1340,-1920,770),16,'Cream')
# 벽면 클라이밍 손잡이: 장식은 비충돌, 실제 올라가기는 기존 발판/점프대를 사용한다.
for i in range(15):
    x=790+(i%3)*130; z=80+(i//3)*115
    shape('Climb_Hold_%02d'%i,'Sphere',(x,-771,z),(55,26,40),['Coral','Blue','Cream'][i%3])

# 탄성 바닥 위 색 구역과 징검다리. 지형 굴곡 구역에는 평면을 덮지 않는다.
for label,x,y,r,color in [('Welcome',-800,-1300,320,'Cream'),('Practice',-700,1000,430,'Lavender'),('Jump',350,-1350,220,'Teal')]:
    shape('RubberZone_'+label,'Cylinder',(x,y,0.5),(r*2,r*2,1.5),color)
for i in range(8):
    shape('Path_Dot_%02d'%i,'Cylinder',(-640+i*125,-1050+i*65,2),(48,48,2),'Yellow')

# 낮은 둘레 울타리와 벤치로 실외 놀이터 분위기를 만든다.
for x in [-1470,1470]:
    rod('Boundary_Rail',(x,-1950,150),(x,1950,150),14,'Teal')
    for y in range(-1900,2000,300):
        rod('Fence_Post',(x,y,120),(x,y,160),16,'Coral')
for y in [-1960,1960]: rod('Boundary_End',(-1470,y,150),(1470,y,150),14,'Teal')
for y in [550,1200]:
    shape('Bench_Seat','Cube',(-1220,y,55),(95,270,20),'Yellow',collision=True)
    shape('Bench_Back','Cube',(-1260,y,105),(15,270,85),'Teal',collision=True)
    for dy in [-90,90]: shape('Bench_Leg','Cylinder',(-1220,y+dy,24),(22,22,48),'Coral',collision=True)

# 원본에 없는 별도 연습 블록만 새로 추가한다. 기존 밀침 물체는 설정을 보존한다.
for i,color in enumerate(['Yellow','Blue','Coral']):
    a=shape('Push_Block_%02d'%i,'Cube',(-600+i*150,1000,60),(90,90,90),color,collision=True)
    a.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    a.static_mesh_component.set_simulate_physics(True)
    a.static_mesh_component.set_mass_override_in_kg('', 15.0, True)

# 열린 미끄럼틀: 2층 데크의 옆에서 지면까지 이어지는 연속 경사면.
points=[(780,-1700,675),(550,-1700,580),(250,-1700,330),(-40,-1700,100),(-220,-1700,25),(-400,-1700,25)]
for i,(a,b) in enumerate(zip(points,points[1:])):
    va=unreal.Vector(*a); vb=unreal.Vector(*b); d=vb-va; mid=(va+vb)*0.5
    shape('Slide_Bed_%02d'%i,'Cube',(mid.x,mid.y,mid.z),(d.length()+4,210,14),'Blue',unreal.MathLibrary.make_rot_from_x(d),collision=True)
    for side in [-1,1]:
        rod('Slide_Edge_%02d_%d'%(i,side),(a[0],a[1]+side*110,a[2]+28),(b[0],b[1]+side*110,b[2]+28),32,'Coral')
shape('Slide_Landing','Cylinder',(-440,-1700,1),(420,420,2),'Yellow')

# 신규 맵에만 시작 위치를 추가하고 원본 캐릭터와 World Settings는 그대로 보존한다.
if not any(isinstance(a,unreal.PlayerStart) for a in original):
    start=actors_api.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-800,-1300,115),unreal.Rotator(pitch=0,yaw=40,roll=0))
    start.set_actor_label('PG_PlayerStart')
assert all(by_label[k].get_class().get_path_name()==v for k,v in original_classes.items())
assert unreal.EditorLoadingAndSavingUtils.save_map(world,TARGET)
actors_api.set_selected_level_actors([])
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-1700,-2400,3000),unreal.Rotator(pitch=-48,yaw=52,roll=0))
report={'map':world.get_path_name(),'original_actor_count':len(original),'added_details':len(created),'original_actor_classes_preserved':True,'play_tested':False}
Path(unreal.Paths.project_saved_dir()+'PlaygroundWork/result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('PLAYGROUND_BUILD_DONE '+json.dumps(report))





