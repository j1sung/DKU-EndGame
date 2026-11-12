"""현재 Round3 외곽 낮은 벽 위에 플레이어 전용 투명 충돌벽을 구간별 추가한다."""
from datetime import datetime
from pathlib import Path
import json
import math
import shutil
import unreal

MAP='/Game/Map/L_Round3';PREFIX='R3D_InvisibleBoundary_'
TOP_Z=3000.;MAX_SEGMENT=400.;OVERLAP=10.
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
actors=api.get_all_level_actors()
assert not any(a.get_actor_label().startswith(PREFIX) for a in actors),'이미 투명 외곽벽이 있습니다.'
walls=[a for a in actors if a.get_actor_label().startswith('R3A_Arena_Boundary')]
assert len(walls)==4,'외곽 벽 네 구간을 확인해야 합니다.'
walls.sort(key=lambda a:(a.get_actor_location().x,a.get_actor_location().y))
original={a.get_path_name():[a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()] for a in actors}
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
root=Path(unreal.Paths.project_dir());out=root/'Saved/Round3Invisible';out.mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'Content/Map/L_Round3.umap',out/('before_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap'))

# EditorActorSubsystem의 Volume 팩터리가 브러시 박스를 생성하는지 확인한다.
probe=api.spawn_actor_from_class(unreal.BlockingVolume,unreal.Vector(0,0,-20000),transient=True)
_,extent=probe.get_actor_bounds(False)
use_volume=min(extent.x,extent.y,extent.z)>.1
api.destroy_actor(probe)
cube=unreal.load_asset('/Engine/BasicShapes/Cube') if not use_volume else None
added=[];report=[]
with unreal.ScopedEditorTransaction('Round3 invisible perimeter collision'):
    for side,wall in enumerate(walls):
        rotation=wall.get_actor_rotation()
        assert abs(rotation.pitch)<.01 and abs(rotation.roll)<.01
        assert abs((rotation.yaw+45)%90-45)<.01,'축에 정렬되지 않은 외곽 벽은 수동 구간 확인 필요'
        origin,e=wall.get_actor_bounds(False)
        along_x=e.x>=e.y
        length=2*(e.x if along_x else e.y)
        thickness=max(50.,2*(e.y if along_x else e.x)+16.)
        bottom=origin.z+e.z-OVERLAP
        assert TOP_Z>bottom+100
        height=TOP_Z-bottom;count=math.ceil(length/MAX_SEGMENT);step=length/count
        for index in range(count):
            offset=-length/2+(index+.5)*step
            location=unreal.Vector(origin.x+(offset if along_x else 0),origin.y+(0 if along_x else offset),(bottom+TOP_Z)/2)
            size=unreal.Vector(step+OVERLAP,thickness,height) if along_x else unreal.Vector(thickness,step+OVERLAP,height)
            a=api.spawn_actor_from_class(unreal.BlockingVolume if use_volume else unreal.StaticMeshActor,location)
            a.set_actor_label(PREFIX+'%02d_%02d'%(side,index));a.set_folder_path('Round3Dam/InvisibleBoundary')
            c=a.get_component_by_class(unreal.BrushComponent if use_volume else unreal.StaticMeshComponent)
            if not use_volume:
                c.set_static_mesh(cube);c.set_visibility(False);c.set_editor_property('use_default_collision',False)
            _,base_extent=a.get_actor_bounds(False)
            assert min(base_extent.x,base_extent.y,base_extent.z)>.1
            a.set_actor_scale3d(unreal.Vector(size.x/(base_extent.x*2),size.y/(base_extent.y*2),size.z/(base_extent.z*2)))
            c.set_collision_profile_name('Custom');c.set_collision_object_type(unreal.CollisionChannel.ECC_WORLD_STATIC)
            c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
            c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN,unreal.CollisionResponseType.ECR_BLOCK)
            c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
            c.set_editor_property('generate_overlap_events',False);c.set_cast_shadow(False)
            a.set_actor_hidden_in_game(True);added.append(a)
            report.append(dict(label=a.get_actor_label(),side=side,bottom_z=bottom,top_z=TOP_Z,size=[size.x,size.y,size.z],along_x=along_x))
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
after={a.get_path_name():a for a in api.get_all_level_actors()}
for path,values in original.items():
    a=after[path]
    assert a.get_actor_location().distance(values[0])<.01
    assert a.get_actor_scale3d().distance(values[2])<.0001
barriers=[a for a in after.values() if a.get_actor_label().startswith(PREFIX)]
assert len(barriers)==len(report)
checks=0
for a in barriers:
    c=a.get_component_by_class(unreal.BrushComponent if use_volume else unreal.StaticMeshComponent)
    assert a.get_editor_property('hidden')
    assert not c.get_editor_property('cast_shadow')
    assert c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)==unreal.CollisionResponseType.ECR_BLOCK
    assert c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA)==unreal.CollisionResponseType.ECR_IGNORE
    origin,e=a.get_actor_bounds(False);along_x=e.x>=e.y
    # 각 구간 중앙/양끝을 낮은 벽 바로 위·중간·꼭대기에서 검사해 틈을 찾는다.
    for fraction in [-.48,0,.48]:
        for z in [origin.z-e.z+20,1500,TOP_Z-20]:
            p=unreal.Vector(origin.x+(e.x*2*fraction if along_x else 0),origin.y+(0 if along_x else e.y*2*fraction),z)
            offset=unreal.Vector(0,180,0) if along_x else unreal.Vector(180,0,0)
            hit=unreal.SystemLibrary.line_trace_single_for_objects(world,p-offset,p+offset,[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1],False,[],unreal.DrawDebugTrace.NONE,True)
            assert hit and hit.to_tuple()[0],a.get_actor_label()
            checks+=1
(out/'verified.json').write_text(json.dumps(dict(type='BlockingVolume' if use_volume else 'HiddenStaticMesh',segments=report,collision_probes=checks,original_actors_preserved=len(original)),indent=2),encoding='utf-8')
unreal.log('R3_INVISIBLE_BOUNDARY_VERIFIED '+str(len(barriers))+' segments; '+str(checks)+' collision probes')
