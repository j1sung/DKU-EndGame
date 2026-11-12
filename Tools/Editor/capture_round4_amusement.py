"""저장된 새 Round4의 탑뷰와 사선 뷰를 실제 에디터 렌더로 캡처한다."""
import json
import time
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/ConceptCaptures/Round4Amusement';OUT.mkdir(parents=True,exist_ok=True)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
camera=api.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(),transient=True)
camera.set_actor_label('TEMP_Round4Capture');c=camera.get_component_by_class(unreal.CameraComponent)
c.set_editor_property('constrain_aspect_ratio',False)
shots=[dict(name='01_Overview',p=(4200,-5700,3700),target=(0,300,900),fov=62),
       dict(name='02_Top',p=(0,-350,8500),target=(0,-350,0),ortho=5300),
       dict(name='03_Arena',p=(2700,-3500,2400),target=(0,-250,700),fov=62)]
state=dict(index=0,task=None,start=time.monotonic(),results=[])


def cleanup():
    api.destroy_actor(camera);unreal.unregister_slate_post_tick_callback(state['handle'])


def tick(dt):
    try:
        if state['task'] is None:
            shot=shots[state['index']];p=unreal.Vector(*shot['p']);target=unreal.Vector(*shot['target'])
            r=unreal.MathLibrary.find_look_at_rotation(p,target)
            if shot.get('ortho'):
                r=unreal.Rotator(pitch=-90,yaw=90);c.set_projection_mode(unreal.CameraProjectionMode.ORTHOGRAPHIC)
                c.set_ortho_width(shot['ortho']);c.set_auto_calculate_ortho_planes(False);c.set_ortho_near_clip_plane(1);c.set_ortho_far_clip_plane(20000)
            else:c.set_projection_mode(unreal.CameraProjectionMode.PERSPECTIVE);c.set_field_of_view(shot['fov'])
            c.set_aspect_ratio(16/9);camera.set_actor_location_and_rotation(p,r,False,False)
            state['file']=str(OUT/(shot['name']+'.png'))
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1920,1080,state['file'],camera=camera,delay=3,force_game_view=True)
            state['start']=time.monotonic();return
        if state['task'].is_task_done() and Path(state['file']).exists():
            state['results'].append(state['file']);state['index']+=1;state['task']=None
            if state['index']==len(shots):
                (OUT/'captured.json').write_text(json.dumps(state['results']),encoding='utf-8');cleanup()
                unreal.log('ROUND4_CAPTURE_COMPLETE')
        elif time.monotonic()-state['start']>90:raise RuntimeError('Capture timeout')
    except Exception:
        (OUT/'error.txt').write_text(traceback.format_exc(),encoding='utf-8');cleanup()


state['handle']=unreal.register_slate_post_tick_callback(tick)
