"""임시 캐릭터로 착지, 유지 시간, 붕괴, 복구를 PIE에서 검사한다. 저장하지 않는다."""
import json,time,traceback
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Round4Floating';MAP='/Game/Map/L_Round4_Amusement'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
pads=sorted([a for a in api.get_all_level_actors() if a.get_actor_label().startswith('R4A_FloatingRefuge_')],key=lambda a:a.get_actor_label())
assert len(pads)==3
pads[2].set_editor_property('stand_duration',1.25)
for i,pad in enumerate(pads):
    p=pad.get_actor_location();p.z+=112 if i!=1 else -148
    a=api.spawn_actor_from_class(unreal.Character,p);a.set_actor_label('TEMP_FloatProbe_'+str(i))
    a.get_component_by_class(unreal.CharacterMovementComponent).set_editor_property('run_physics_with_no_controller',True)
report={'samples':[]};state={'end':None,'moved':set(),'phase':[0,0,0]};start=time.monotonic()
def phase(p,i,characters):
    c=p.get_editor_property('platform')
    if c.get_editor_property('hidden_in_game'):result=3
    elif c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION:result=2
    elif state['phase'][i] in [2,3]:result=0
    elif any(abs((a.get_actor_location().z-a.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height())-(p.get_actor_location().z+12))<5 and abs(a.get_actor_location().x-p.get_actor_location().x)<40 and abs(a.get_actor_location().y-p.get_actor_location().y)<80 and abs(a.get_velocity().z)<2 for a in characters):result=1
    else:result=state['phase'][i]
    state['phase'][i]=result
    return result
def tick(dt):
    try:
        w=unreal.EditorLevelLibrary.get_game_world()
        if state['end'] is not None:
            if w or time.monotonic()-state['end']<1:return
            unreal.unregister_slate_post_tick_callback(state['handle'])
            unreal.EditorLoadingAndSavingUtils.load_map(MAP)
            report['restored_duration']=[a.get_editor_property('stand_duration') for a in api.get_all_level_actors() if a.get_actor_label().startswith('R4A_FloatingRefuge_')]
            assert report['restored_duration']==[3,3,3]
            (OUT/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
            unreal.log('ROUND4_FLOATING_VERIFIED '+str(report.get('error')));return
        if time.monotonic()-start>90:raise RuntimeError('PIE timeout')
        if not w:return
        t=unreal.GameplayStatics.get_time_seconds(w)
        if report['samples'] and t-report['samples'][-1]['t']<.05:return
        actors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)
        pp=sorted([a for a in actors if a.get_actor_label().startswith('R4A_FloatingRefuge_')],key=lambda a:a.get_actor_label())
        if len(pp)!=3:return
        characters=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Character)
        phases=[phase(a,i,characters) for i,a in enumerate(pp)]
        comps=[a.get_editor_property('platform') for a in pp]
        report['samples'].append(dict(t=t,phase=phases,collision=[str(c.get_collision_enabled()) for c in comps],z=[c.get_editor_property('relative_location').z for c in comps]))
        # 밟고 떠나도 시간이 계속 가는지 확인한다. 아래 캐릭터는 4초 뒤 치운다.
        for i in [0,1,2]:
            trigger=(i==1 and t>4) or (i!=1 and phases[i]==1 and t>1)
            if trigger and i not in state['moved']:
                a=next(a for a in actors if a.get_actor_label()=='TEMP_FloatProbe_'+str(i))
                a.set_actor_location(unreal.Vector(4000,4000,400),False,True);state['moved'].add(i)
        if t>11.5:
            rows=report['samples']
            for i,duration in [(0,3),(2,1.25)]:
                first=next(r['t'] for r in rows if r['phase'][i]==1)
                dropped=next(r['t'] for r in rows if r['phase'][i]==2)
                hidden=next(r['t'] for r in rows if r['phase'][i]==3)
                restored=next(r['t'] for r in rows if r['t']>hidden and r['phase'][i]==0)
                assert abs(dropped-first-duration)<.2,(i,dropped-first)
                assert abs(restored-hidden-6)<.2
                assert any(r['z'][i]<-10 for r in rows if r['phase'][i]==2)
                assert all('NO_COLLISION' in r['collision'][i] for r in rows if r['phase'][i]>=2)
            assert all(r['phase'][1]==0 for r in rows if r['t']<4),'아래 접촉으로 발판 활성화'
            report['error']=None;unreal.EditorLevelLibrary.editor_end_play();state['end']=time.monotonic()
    except Exception:
        report['error']=traceback.format_exc();unreal.EditorLevelLibrary.editor_end_play();state['end']=time.monotonic()
state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.EditorLevelLibrary.editor_play_simulate()

