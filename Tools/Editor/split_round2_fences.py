"""Organic 맵의 일체형 울타리만 삭제·이동 가능한 개별 패널 액터로 교체한다."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT/'Saved/Round2OrganicWork'
MAP = '/Game/Map/L_Round2_Organic'
BASE = '/Game/LD_Gym/LDProps/StaticMeshes/Round2Organic'
api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0] == MAP
actors = {a.get_actor_label():a for a in api.get_all_level_actors()}
assert not any(k.startswith('OR_FencePanel_') for k in actors), '이미 패널로 분리된 맵입니다.'
groups = {g:actors['OR_Fence_'+g] for g in ['Outer','Hub','Stairs','Lift']}
old = [a for k,a in actors.items() if k in ['OR_Fence_'+g for g in groups] or k.startswith(('OR_EdgeGuard_','OR_LiftGuard_'))]
assert len(old) == 56, '교체할 기존 울타리와 충돌 액터 구성을 확인하세요.'
source_hash = hashlib.sha256((ROOT/'Content/Map/L_Round2.umap').read_bytes()).hexdigest()

def snapshot(a):
    p,r,s = a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()
    return {'loc':[p.x,p.y,p.z],'rot':[r.pitch,r.yaw,r.roll],'scale':[s.x,s.y,s.z],
            'meshes':[c.static_mesh.get_path_name() for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]}

preserved = {k:snapshot(a) for k,a in actors.items() if a not in old}
spec = importlib.util.spec_from_file_location('round2_organic_geometry',ROOT/'Tools/Editor/round2_organic_geometry.py')
geometry = importlib.util.module_from_spec(spec)
spec.loader.exec_module(geometry)
if (OUT/'applied.json').exists():
    segments = json.loads((OUT/'applied.json').read_text(encoding='utf-8'))['fences']['segments']
else:
    # 생성 자료가 없는 체크아웃에서는 보존된 그레이박스의 보행면을 다시 조회한다.
    triangles = []
    for label,a in actors.items():
        if not label.startswith('SM_Dummy_'):
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
    segments = geometry.make_sources(triangles,OUT/'MeshSources')['segments']

panels,definitions = [],{}
def append_panel(group,label,a,b,height=360):
    # 이동한 일체형 울타리가 있으면 현재 액터의 변환도 반영한다.
    actor = groups[group]
    rotation = actor.get_actor_rotation()
    assert abs(rotation.pitch)+abs(rotation.roll) < .01, '기울어진 일체형 울타리는 먼저 회전을 확인하세요.'
    tx = actor.get_actor_transform()
    a = unreal.MathLibrary.transform_location(tx,unreal.Vector(*a))
    b = unreal.MathLibrary.transform_location(tx,unreal.Vector(*b))
    scale = actor.get_actor_scale3d()
    height *= scale.z
    width = math.hypot(b.x-a.x,b.y-a.y)
    rise = b.z-a.z
    # 모든 평지 패널은 하나의 메시를 공유한다. 경사 패널만 상승량별 메시를 사용한다.
    key = 0.0 if abs(rise)<.05 else round(rise*360/height,1)
    name = 'SM_Round2_FencePanel_Flat' if key == 0 else 'SM_Round2_FencePanel_Rise_'+('P' if key>0 else 'N')+str(abs(key)).replace('.','_')
    definitions[name] = key
    panels.append({'label':'OR_FencePanel_'+label,'group':group,'mesh':name,
                   'location':[(a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2],
                   'yaw':math.degrees(math.atan2(b.y-a.y,b.x-a.x)),
                   'scale':[width/240,1,height/360],'width':width,'height':height})

for i,segment in enumerate(segments):
    a,b = segment['a'],segment['b']
    count = max(1,math.ceil(math.dist(a[:2],b[:2])/240))
    for j in range(count):
        aa = [a[k]+(b[k]-a[k])*j/count for k in range(3)]
        bb = [a[k]+(b[k]-a[k])*(j+1)/count for k in range(3)]
        append_panel(segment['group'],'%s_%02d_%02d'%(segment['group'],i,j),aa,bb)
# 승강기 울타리도 가로·세로 네모 모듈로 나눈다. 각 네모의 충돌은 그 높이에만 존재한다.
for i,(a,b) in enumerate([((-2810,-2430,965),(-1555,-2430,965)),
                         ((-2810,-2430,965),(-2810,-1210,965)),
                         ((-1555,-2430,965),(-1555,-1210,965))]):
    columns = math.ceil(math.dist(a[:2],b[:2])/240)
    rows = math.ceil(1955/360)
    for row in range(rows):
        for col in range(columns):
            aa = [a[k]+(b[k]-a[k])*col/columns for k in range(3)]
            bb = [a[k]+(b[k]-a[k])*(col+1)/columns for k in range(3)]
            aa[2] += row*1955/rows
            bb[2] += row*1955/rows
            append_panel('Lift','Lift_%02d_%02d_%02d'%(i,row,col),aa,bb,1955/rows)

folder = OUT/'FencePanelSources'
folder.mkdir(parents=True,exist_ok=True)
(folder/'organic.mtl').write_text('newmtl Neutral\nKd .3 .32 .34\n')
tasks = []
for name,rise in definitions.items():
    path = BASE+'/Meshes/FencePanels/'+name
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), '기존 패널 에셋을 덮어쓰지 않습니다: '+name
    m = geometry.Mesh(name)
    def p(x,z):
        return (x,0,z+rise*x/240)
    for x in [-120,120]:
        m.rod(p(x,0),p(x,360),6)
    for z in [35,180,360]:
        m.rod(p(-120,z),p(120,z),3.5)
    for x in [-80,0,80]:
        m.rod(p(x,35),p(x,360),1.2,4)
    for z in [100,260]:
        m.rod(p(-120,z),p(120,z),1.2,4)
    source = m.save(folder)
    task = unreal.AssetImportTask()
    task.filename,task.destination_path,task.destination_name = str(source),BASE+'/Meshes/FencePanels',name
    task.automated,task.save = True,False
    options = unreal.FbxImportUI()
    options.import_materials,options.import_textures = False,False
    options.static_mesh_import_data.combine_meshes = True
    task.options = options
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
mesh_api = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
material = unreal.load_asset(BASE+'/Materials/M_Round2_Guard')
meshes = {}
for name in definitions:
    mesh = unreal.load_asset(BASE+'/Meshes/FencePanels/'+name)
    assert mesh
    mesh_api.remove_collisions(mesh)
    mesh_api.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    mesh.set_material(0,material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    meshes[name] = mesh

with unreal.ScopedEditorTransaction('Split Round2 fences into independent panels'):
    for panel in panels:
        a = api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*panel['location']),unreal.Rotator(yaw=panel['yaw']))
        a.set_actor_label(panel['label'])
        a.set_folder_path('Round2Organic/FencePanels/'+panel['group'])
        c = a.static_mesh_component
        c.set_static_mesh(meshes[panel['mesh']])
        a.set_actor_scale3d(unreal.Vector(*panel['scale']))
        c.modify()
        c.set_editor_property('use_default_collision',False)
        c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        c.set_collision_profile_name('Custom')
        c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
        c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN,unreal.CollisionResponseType.ECR_BLOCK)
        c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PHYSICS_BODY,unreal.CollisionResponseType.ECR_BLOCK)
    # 거대한 시각 메시와 별도 투명 벽을 함께 제거해 패널 삭제만으로 통로가 열리게 한다.
    for a in old:
        assert api.destroy_actor(a)
    api.set_selected_level_actors([])
    assert all(snapshot(actors[k])==value for k,value in preserved.items()), '울타리 외 액터 변경 감지'
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
assert hashlib.sha256((ROOT/'Content/Map/L_Round2.umap').read_bytes()).hexdigest() == source_hash
(OUT/'fence-panels.json').write_text(json.dumps({'map':MAP,'panels':panels,'meshes':list(definitions),
    'preserved':preserved,'source_hash':source_hash,'old_fence_actors_removed':len(old)},indent=2),encoding='utf-8')
unreal.log('ROUND2_FENCE_PANELS_SAVED '+str(len(panels)))
