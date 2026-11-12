"""열린 Round3의 사용자 변환·기믹 설정을 보존하며 댐 정비장 테마를 한 번 적용한다."""
import hashlib
import json
import math
import shutil
from pathlib import Path
from datetime import datetime
import unreal

ROOT=Path(unreal.Paths.project_dir()); WORK=ROOT/'Saved/Round3Theme'; SRC=WORK/'Sources'
BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round3'; MAP='/Game/Map/L_Round3'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors=list(api.get_all_level_actors()); before=json.loads((WORK/'before.json').read_text(encoding='utf-8'))
assert len(actors)==len(before),'조사 후 액터가 변경됨'
assert not any(a.get_actor_label().startswith('R3D_') for a in actors),'이미 테마가 적용됨'
for a,row in zip(actors,before):
    assert a.get_path_name()==row['path']
    p=a.get_actor_location();assert math.dist([p.x,p.y,p.z],row['loc'])<.01,'사용자 배치가 변경됨'
assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Materials/M_R3D_Concrete'),'기존 전용 에셋 덮어쓰기 금지'
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
shutil.copy2(ROOT/'Content/Map/L_Round3.umap',WORK/('before_theme_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap'))
other_hash={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/Map').glob('L_Round*.umap') if p.name!='L_Round3.umap'}
manifest=json.loads((SRC/'manifest.json').read_text(encoding='utf-8'))
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
mesh_api=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
mats={};meshes={};added=[]


def const(m,value):
    e=lib.create_material_expression(m,unreal.MaterialExpressionConstant);e.set_editor_property('r',value);return e


def custom(m,code,inputs,output=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    e=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    e.set_editor_property('code',code);e.set_editor_property('output_type',output)
    definitions=[]
    for name,expr in inputs.items():
        definition=unreal.CustomInput();definition.set_editor_property('input_name',name);definitions.append(definition)
    e.set_editor_property('inputs',definitions)
    for name,expr in inputs.items():assert lib.connect_material_expressions(expr,'',e,name)
    return e


def material(key,rgb):
    m=tools.create_asset('M_R3D_'+key,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew());assert m
    pos=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
    color='float3(%s)'%','.join(map(str,rgb))
    common='float n=sin(dot(P,float3(.127,.231,.173)))*sin(dot(P,float3(.71,.13,.37)));'
    if key in ['Concrete','Coping','Rock']:
        code=common+'''float broad=sin(P.x*.007+sin(P.y*.009))*sin(P.z*.011+P.y*.004);
        float grain=n*.022+broad*.045;
        return max(0,COLOR+grain);'''.replace('COLOR',color)
    elif key in ['Wood','WoodLight']:
        code=common+'''float grain=sin(P.y*.36+sin(P.x*.017)*2+sin(P.z*.12))*.032;
        return max(.01,COLOR+grain+n*.012);'''.replace('COLOR',color)
    else:code=common+'return max(.005,'+color+'+n*.012);'
    lib.connect_material_property(custom(m,code,{'P':pos}),'',unreal.MaterialProperty.MP_BASE_COLOR)
    metal=.7 if key in ['Steel','Teal','Orange','Yellow'] else 0
    lib.connect_material_property(const(m,metal),'',unreal.MaterialProperty.MP_METALLIC)
    rough=.33 if key=='Teal' else .48 if metal else .85
    lib.connect_material_property(const(m,rough),'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(m);mats[key]=m
    return m


for key,rgb in manifest['palette'].items():
    if key!='Water':material(key,rgb)

# 얕은 웅덩이: 실제 조명을 받는 투명 표면 + 시간에 따라 변하는 노멀.
# 위치 이동/물리 충돌 없이 바닥만 젖어 보이므로 기존 전투 판정을 유지한다.
for key,jet in [('Water',False),('JetWater',True)]:
    m=tools.create_asset('M_R3D_'+key,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew());assert m
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    p=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
    t=lib.create_material_expression(m,unreal.MaterialExpressionTime)
    n=custom(m,'float a=P.x*.095+P.y*.027+T*1.1; float b=P.y*.12-P.x*.035-T*.8; return normalize(float3(cos(a)*.045+sin(b)*.025,sin(a)*.03+cos(b)*.045,1));',{'P':p,'T':t})
    lib.connect_material_property(n,'',unreal.MaterialProperty.MP_NORMAL)
    col=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    col.set_editor_property('constant',unreal.LinearColor(.025,.21,.18,1) if not jet else unreal.LinearColor(.25,.65,.63,1))
    lib.connect_material_property(col,'',unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(const(m,.07 if not jet else .14),'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(const(m,.75),'',unreal.MaterialProperty.MP_SPECULAR)
    if jet:
        opacity=custom(m,'return .17+.3*pow(saturate(sin(P.z*.16-T*8+sin(P.x*.2))),5);',{'P':p,'T':t},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    else:
        f=lib.create_material_expression(m,unreal.MaterialExpressionFresnel)
        opacity=lib.create_material_expression(m,unreal.MaterialExpressionMultiply);opacity.set_editor_property('const_b',.5);lib.connect_material_expressions(f,'',opacity,'A')
        add=lib.create_material_expression(m,unreal.MaterialExpressionAdd);add.set_editor_property('const_b',.32);lib.connect_material_expressions(opacity,'',add,'A');opacity=add
    lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);mats[key]=m

tasks=[]
for row in manifest['meshes']:
    task=unreal.AssetImportTask();task.filename=str(SRC/(row['name']+'.obj'));task.destination_path=BASE;task.destination_name=row['name'];task.automated=True;task.save=False
    opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;task.options=opt;tasks.append(task)
tools.import_asset_tasks(tasks)
for row in manifest['meshes']:
    mesh=unreal.load_asset(BASE+'/'+row['name']);assert mesh,row['name']
    for i,slot in enumerate(mesh.static_materials):mesh.set_material(i,mats[str(slot.material_slot_name)])
    mesh_api.remove_collisions(mesh)
    if row['collision']!='none':
        mesh_api.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX if row['collision']=='box' else unreal.ScriptCollisionShapeType.NDOP26)
    if row['nanite']:
        settings=mesh_api.get_nanite_settings(mesh);settings.set_editor_property('enabled',True);mesh_api.set_nanite_settings(mesh,settings,True)
    meshes[row['name']]=mesh


def paint(a,key):
    a.modify()
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if not c.static_mesh:continue
        c.modify()
        for i in range(c.get_num_materials()):c.set_material(i,mats[key])


def replace(a,name):
    a.modify();c=a.get_component_by_class(unreal.StaticMeshComponent);c.modify();c.set_static_mesh(meshes[name])
    # 이전 슬롯의 재질 오버라이드가 새 메시의 목재/철제 슬롯을 덮지 않게 초기화한다.
    c.set_editor_property('override_materials',[])


def spawn(name,loc,scale=(1,1,1),yaw=0,folder='Equipment',parent=None):
    a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(yaw=yaw));a.set_actor_label('R3D_'+name.replace('SM_R3D_','')+'_%03d'%len(added));a.set_folder_path('Round3Dam/'+folder)
    c=a.static_mesh_component;c.set_static_mesh(meshes[name]);c.set_editor_property('use_default_collision',False);c.set_collision_profile_name('NoCollision')
    if parent:
        c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_absolute(False,False,True)
        assert a.attach_to_component(parent.root_component,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
    a.set_actor_scale3d(unreal.Vector(*scale))
    if 'Puddle' in name:c.set_cast_shadow(False);c.set_editor_property('translucency_sort_priority',1)
    added.append(a);return a


with unreal.ScopedEditorTransaction('Round3 dam maintenance theme'):
    for a in actors:
        label=a.get_actor_label();c=a.get_component_by_class(unreal.StaticMeshComponent)
        if label=='R3A_PreviewSky':continue
        if label.startswith('R3L_Bridge_Plank'):replace(a,'SM_R3D_BridgePlank_100')
        elif label.startswith(('R3L_Ramp_','R3A_Arena_Hill')):replace(a,'SM_R3D_RampDeck_100')
        elif label.startswith('R3A_DropBlock'):replace(a,'SM_R3D_CargoCrate_100')
        elif label=='R3M_Sweeper':replace(a,'SM_R3D_Turntable_100')
        elif label=='R3M_Sweeper_Bar':replace(a,'SM_R3D_SweeperBar_100')
        elif label.startswith('R3L_Bridge_Rope'):paint(a,'Wood')
        elif label.startswith(('R3L_Bridge_Post','R3L_Upper_Post','R3L_Upper_Rail')):paint(a,'Orange')
        elif label.startswith('R3L_Jet_Water'):paint(a,'JetWater')
        elif label.startswith('R3M_LogSpawner'):paint(a,'WoodLight')
        elif label.startswith(('R3A_Ball','R3A_Pendulum')):paint(a,'Yellow')
        elif label.startswith('R3A_Arena_DropMarker'):paint(a,'Yellow')
        elif label.startswith('R3A_Arena_SpawnMark'):paint(a,'Teal')
        elif label=='R3M_Sweeper_Hub':paint(a,'Yellow')
        elif c and c.static_mesh and label.startswith(('R3A_Arena_','R3L_Upper_','R3L_Stairs_')):
            if c.static_mesh.get_path_name().startswith('/Engine/BasicShapes/Cube'):replace(a,'SM_R3D_ConcretePanel_100')
            else:paint(a,'Concrete')

    # 외곽 벽 밖에 배관을 놓아 통로 폭을 줄이지 않는다. 각 모듈은 개별 액터다.
    for x in [-1670,1670]:
        for i in range(10):
            y=-1800+i*390;spawn('SM_R3D_Pipe_400',(x,y,270),(.975,1,1),90,'Pipes')
            spawn('SM_R3D_PipeBracket',(x,y,265),yaw=90 if x<0 else -90,folder='Pipes')
    for y in [-2050,1960]:
        for i in range(8):spawn('SM_R3D_Pipe_400',(-1400+i*400,y,270),folder='Pipes')
    for x,y,yaw in [(1610,1900,0),(-1610,1900,90),(-1610,-1990,180),(1610,-1990,270)]:spawn('SM_R3D_PipeCorner',(x,y,270),yaw=yaw,folder='Pipes')
    for p,yaw in [((-1480,1530,0),90),((1470,-1680,0),90),((400,1780,0),0)]:spawn('SM_R3D_ValveStation',p,yaw=yaw)
    # 바닥 이음매와 배수구는 기존 바닥 위 1cm 이내에 둔다.
    for x in [-1200,-800,-400,400,800,1200]:
        for y in [-1740,-1340,-940,660,1060,1460]:spawn('SM_R3D_ExpansionJoint_400',(x,y,.15),folder='FloorDetails')
    for p in [(-1450,-1600,.5),(1450,1600,.5),(-1450,1500,.5),(1450,-1700,.5)]:spawn('SM_R3D_Drain_100',p,folder='FloorDetails')
    for row in before:
        if row['label'].startswith('R3L_JetCycle_'):
            x,y,_=row['loc'];spawn('SM_R3D_JetNozzle_170',(x,y,0),folder='Water')
    puddles=[(600,1280,1.4,1.1,30),(-250,-1320,1.6,.9,10),(-750,-860,1.2,.8,80),(840,440,1.3,.9,20),(-1180,1400,1.25,.8,80),(1330,-1510,1.1,.9,30),(180,1600,1,.75,0),(1300,250,.7,.7,0)]
    for i,(x,y,sx,sy,yaw) in enumerate(puddles):spawn('SM_R3D_Puddle_'+str(i%3),(x,y,.7),(sx,sy,1),yaw,'Water')
    # 수조 하부의 콘크리트 외벽은 플레이 공간 아래에만 추가한다.
    for x in [-1610,1610]:spawn('SM_R3D_ConcretePanel_100',(x,-45,-460),(.45,39.3,8.8),folder='Foundation')
    for y in [-1990,1900]:spawn('SM_R3D_ConcretePanel_100',(0,y,-460),(32.2,.45,8.8),folder='Foundation')
    # 뒤쪽 암벽은 높게, 카메라 앞쪽은 낮게 두어 전투 공간이 가려지지 않게 한다.
    for i in range(9):
        y=-2400+i*600
        for side in [-1,1]:
            x=side*(2410+100*math.sin(i*2.1));height=.7 if side>0 else 1.15
            spawn('SM_R3D_Cliff_'+str(i%3),(x,y,-400),(.95,1.1,height),i*37,'Cliffs')
    for i in range(7):
        x=-2200+i*700
        for side in [-1,1]:spawn('SM_R3D_Cliff_'+str((i+1)%3),(x,side*2750,-560),(1.1,1,.5 if side<0 else 1.25),i*61,'Cliffs')

for asset in unreal.EditorAssetLibrary.list_assets(BASE,True,False):
    assert unreal.EditorAssetLibrary.save_asset(asset,only_if_is_dirty=True)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
for a,row in zip(actors,before):
    p=a.get_actor_location();s=a.get_actor_scale3d();r=a.get_actor_rotation()
    assert math.dist([p.x,p.y,p.z],row['loc'])<.01
    assert math.dist([s.x,s.y,s.z],row['scale'])<.0001
    assert math.dist([r.pitch,r.yaw,r.roll],row['rot'])<.001
for p in (ROOT/'Content/Map').glob('L_Round*.umap'):
    if p.name in other_hash:assert hashlib.sha256(p.read_bytes()).hexdigest()==other_hash[p.name]
(WORK/'applied.json').write_text(json.dumps(dict(original_actors=len(actors),added_actors=len(added),meshes=len(meshes),materials=len(mats),other_maps_unchanged=True),indent=2),encoding='utf-8')
unreal.log('R3_DAM_THEME_APPLIED')
