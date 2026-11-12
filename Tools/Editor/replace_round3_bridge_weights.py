"""다리 위 왕복 구체 두 개를 기존 화물 상자 메시로 교체한다."""
from datetime import datetime
from pathlib import Path
import json
import shutil
import unreal

MAP='/Game/Map/L_Round3'
MESH='/Game/LD_Gym/LDProps/StaticMeshes/Round3/SM_R3D_CargoCrate_100'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
labels=['R3A_Ball_Proxy','R3A_Ball_Proxy2']
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
targets=[actors[label] for label in labels]
mesh=unreal.load_asset(MESH);assert mesh


def snapshot(a):
    p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d()
    result=dict(location=[p.x,p.y,p.z],rotation=[r.pitch,r.yaw,r.roll],scale=[s.x,s.y,s.z],movement=[])
    for c in a.get_components_by_class(unreal.InterpToMovementComponent):
        points=[]
        for point in c.get_editor_property('control_points'):
            v=point.get_editor_property('position_control_point')
            points.append([v.x,v.y,v.z,point.get_editor_property('position_is_relative')])
        result['movement'].append(dict(duration=c.get_editor_property('duration'),behaviour=str(c.get_editor_property('behaviour_type')),points=points))
    return result


before={a.get_actor_label():snapshot(a) for a in targets}
assert all(row['movement'] for row in before.values())
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
root=Path(unreal.Paths.project_dir());out=root/'Saved/Round3Theme'
shutil.copy2(root/'Content/Map/L_Round3.umap',out/('before_bridge_crates_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap'))
with unreal.ScopedEditorTransaction('Replace Round3 bridge weights with cargo crates'):
    for a in targets:
        a.modify();c=a.get_component_by_class(unreal.StaticMeshComponent);c.modify()
        c.set_static_mesh(mesh);c.set_editor_property('override_materials',[])
        assert snapshot(a)==before[a.get_actor_label()]
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
# 저장된 인스턴스를 다시 열어 이동 설정과 메시 참조를 확인한다.
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
for label in labels:
    assert actors[label].get_component_by_class(unreal.StaticMeshComponent).static_mesh.get_path_name().split('.')[0]==MESH
    after=snapshot(actors[label]);old=before[label]
    assert after['movement']==old['movement'] and after['scale']==old['scale']
    assert all(abs(x-y)<.001 for key in ['location','rotation'] for x,y in zip(after[key],old[key]))
(out/'bridge_crates_verified.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
unreal.log('R3_BRIDGE_CRATES_SAVED_VERIFIED')
