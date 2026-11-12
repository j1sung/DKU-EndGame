"""새 맵의 저장 상태·동선 충돌·외곽벽 및 두 번의 시소 동작을 에디터에서 확인한다."""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Round4Amusement';MAP='/Game/Map/L_Round4_Amusement'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=api.get_all_level_actors();by={a.get_actor_label():a for a in actors}
rig=by['R4A_SeesawRig'];deck=rig.get_editor_property('deck')
assert deck.static_mesh.get_path_name().startswith('/Game/LD_Gym/LDProps/StaticMeshes/Round4/')
assert rig.get_editor_property('push_interval')==10
configured_angle=rig.get_editor_property('maximum_tilt_angle')
assert configured_angle>1,'동작 검사는 1도보다 큰 설정에서 실행합니다.'
barriers=[a for a in actors if a.get_actor_label().startswith('R4A_InvisibleBoundary_')]
assert len(barriers)==44
checks=0
for a in barriers:
    c=a.get_component_by_class(unreal.BrushComponent)
    assert a.get_editor_property('hidden') and not c.get_editor_property('cast_shadow')
    assert c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)==unreal.CollisionResponseType.ECR_BLOCK
    assert c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA)==unreal.CollisionResponseType.ECR_IGNORE
    p,e=a.get_actor_bounds(False);along_x=e.x>e.y
    for f in [-.48,0,.48]:
        for z in [130,1100,2980]:
            q=unreal.Vector(p.x+(e.x*2*f if along_x else 0),p.y+(0 if along_x else e.y*2*f),z)
            d=unreal.Vector(0,100,0) if along_x else unreal.Vector(100,0,0)
            hit=unreal.SystemLibrary.line_trace_single_for_objects(world,q-d,q+d,[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1],False,[],unreal.DrawDebugTrace.NONE,True)
            assert hit and hit.to_tuple()[0],a.get_actor_label();checks+=1

walking_points=[(0,0,975),(1000,0,975),(-1000,0,975),(1230,0,975),(-1230,0,975)]
for side in [-1,1]:
    for i in [0,10,25,49]:walking_points.append((side*1230,-1370+(i+.5)*25,(i+1)*19.5))
for x,y,z in walking_points:
    hit=unreal.SystemLibrary.line_trace_single_for_objects(world,unreal.Vector(x,y,z+10),unreal.Vector(x,y,z-10),[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1,unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2],False,[],unreal.DrawDebugTrace.NONE,True)
    assert hit and hit.to_tuple()[0],str((x,y,z))
# 가장 낮은 판 모서리와 수납된 푸셔의 기하 여유를 확인한다.
corner=850-900*math.sin(math.pi/4)+(100-25.2)*math.cos(math.pi/4)
inactive=850-590*math.tan(math.pi/4)+(100-25.2)/math.cos(math.pi/4)-140
assert corner>100 and inactive>100
report=dict(actors=len(actors),barrier_probes=checks,walk_surface_probes=len(walking_points),
            lowest_deck_corner_z=corner,inactive_pusher_clearance_cm=inactive,samples=[])
started=time.monotonic()


def finish(error=None):
    report['error']=error
    unreal.unregister_slate_post_tick_callback(state['handle'])
    (OUT/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.EditorLevelLibrary.editor_end_play()
    unreal.log('ROUND4_AMUSEMENT_VERIFIED '+str(error))


def tick(dt):
    try:
        if time.monotonic()-started>100:finish('PIE timeout');return
        w=unreal.EditorLevelLibrary.get_game_world()
        if not w:return
        t=unreal.GameplayStatics.get_time_seconds(w)
        if report['samples'] and t-report['samples'][-1]['t']<.1:return
        rr=next((a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor) if a.get_actor_label()=='R4A_SeesawRig'),None)
        if not rr:return
        parts={c.get_name():c for c in rr.get_components_by_class(unreal.StaticMeshComponent)}
        angle=rr.get_editor_property('current_tilt_angle')
        row=dict(t=t,angle=angle,boarding_open=rr.get_editor_property('boarding_open'),
                 right_head_z=parts['PusherHead1'].get_editor_property('relative_location').z,
                 left_head_z=parts['PusherHead0'].get_editor_property('relative_location').z,
                 right_bridge_x=parts['BoardingBridge1'].get_editor_property('relative_location').x,
                 right_gate_y=parts['BoardingGate1'].get_editor_property('relative_location').y,
                 characters=[dict(name=a.get_name(),z=a.get_actor_location().z) for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Character)])
        if t>rig.get_editor_property('boarding_travel_time')+.1:
            assert not row['boarding_open'],'닫힘 완료 뒤 출입부가 다시 열림'
            assert abs(row['right_bridge_x']-1220)<.01
            for gate in ['BoardingGate0','BoardingGate1']:
                assert abs(parts[gate].get_editor_property('relative_location').y)<.01
                assert parts[gate].get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
        report['samples'].append(row)
        if abs(angle)>1:
            assert not row['boarding_open']
            assert row['right_bridge_x']>1219
        if t>28:
            angles=[r['angle'] for r in report['samples']]
            assert max(angles)>configured_angle-.1 and min(angles)<-configured_angle+.1,'양쪽 최대 기울기에 도달하지 못함'
            first=next(r['t'] for r in report['samples'] if r['angle']>1)
            second=next(r['t'] for r in report['samples'] if r['angle']<-1)
            assert abs(second-first-10)<.3,'교대 시작 간격 오류'
            assert all(abs(r['angle'])<.01 for r in report['samples'] if r['t']<10)
            assert not row['boarding_open'],'복귀 후 게이트가 열림'
            closing=[r for r in report['samples'] if 1000.1<r['right_bridge_x']<1219.9]
            assert closing,'시작 시 연결 다리 닫힘 애니메이션을 관측하지 못함'
            assert any(.1<r['right_gate_y']<234.9 for r in closing),'게이트 닫힘 애니메이션을 관측하지 못함'
            before_motion=next(r for r in report['samples'] if r['t']>5)
            assert len(before_motion['characters'])==4
            assert all(c['z']>1000 for c in before_motion['characters']),'수평 판에서 캐릭터가 바닥으로 빠짐'
            moving=next(r for r in report['samples'] if r['angle']>30)
            heights=[c['z'] for c in moving['characters']]
            assert max(heights)-min(heights)>200,'캐릭터가 움직이는 판의 높이를 따라가지 않음'
            report['characters_stand_on_level_deck']=True
            report['characters_follow_tilt_height']=True
            report['alternation_interval_measured']=second-first
            report['character_count_first_sample']=len(report['samples'][0]['characters'])
            finish()
    except Exception:finish(traceback.format_exc())


state={'handle':unreal.register_slate_post_tick_callback(tick)}
unreal.EditorLevelLibrary.editor_play_simulate()
