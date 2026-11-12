"""임시 플레이 월드에서 네 방향, 카메라 회전, 단일 표시와 왕복 동작을 검사한다."""
import json,time,traceback
from pathlib import Path
import unreal
OUT=Path(unreal.Paths.project_dir())/'Saved/DangerArrow';report={'checks':[]};state={'end':None,'index':0,'phase':'apply','at':0,'motion':[]};started=time.monotonic()
# 메시 기울기 방향의 월드 yaw, 카메라 yaw, 예상 화면 방향.
cases=[(0,0,0),(90,0,1),(180,0,2),(270,0,3),(0,90,3),(0,180,2),(0,270,1)]
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
editor_world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
map_path=editor_world.get_path_name().split('.')[0]
camera_template=api.spawn_actor_from_class(unreal.CameraActor,unreal.Vector())
camera_template.set_actor_label('TEMP_DangerArrowCamera')
names=['DangerArrowUp','DangerArrowRight','DangerArrowDown','DangerArrowLeft']
def finish(error=None):
 report['error']=error;unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();state['end']=time.monotonic()
def tick(dt):
 try:
  w=unreal.EditorLevelLibrary.get_game_world()
  if state['end'] is not None:
   if w or time.monotonic()-state['end']<1:return
   unreal.unregister_slate_post_tick_callback(state['handle']);api.destroy_actor(camera_template);unreal.EditorLoadingAndSavingUtils.load_map(map_path);(OUT/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8');return
  if time.monotonic()-started>40:finish('PIE timeout');return
  if not w:return
  pc=unreal.GameplayStatics.get_player_controller(w,0)
  if not pc:return
  pawn=pc.get_controlled_pawn()
  if not isinstance(pawn,unreal.ChickenCharacter):return
  widgets=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.CFDangerOverlayWidget,False)
  widgets=[a for a in widgets if a.is_in_viewport()]
  if not widgets:return
  assert len(widgets)==1
  widget=widgets[0]
  images=[a for a in unreal.ObjectIterator(unreal.Image) if a.get_path_name().startswith(widget.get_path_name()+'.')]
  arrows=[next((a for a in images if a.get_name()==n),None) for n in names];assert all(arrows),[a.get_path_name() for a in images]
  t=unreal.GameplayStatics.get_time_seconds(w)
  if 'camera' not in state:
   pawn.set_actor_tick_enabled(False);pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
   state['camera']=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CameraActor) if a.get_actor_label()=='TEMP_DangerArrowCamera')
   state['camera'].set_actor_location(pawn.get_actor_location()+unreal.Vector(-600,0,180),False,True)
   pc.set_view_target_with_blend(state['camera'],0)
   pres=pawn.get_component_by_class(unreal.CFPlayerPresentationComponent);report['initial_margin']=pres.get_editor_property('danger_warning_angle_margin')
  if state['phase']=='apply':
   if state['index']==len(cases):
    pawn.start_knockdown(unreal.CFKnockdownDirection.FORWARD);state['phase']='death';state['at']=t;return
   yaw,cam,expected=cases[state['index']]
   pawn.set_editor_property('body_tilt',unreal.Vector2D(55/35,0))
   pawn.get_component_by_class(unreal.SkeletalMeshComponent).set_world_rotation(unreal.Rotator(pitch=-55,yaw=yaw),False,True)
   state['camera'].set_actor_rotation(unreal.Rotator(pitch=-15,yaw=cam),True)
   state['at']=t;state['motion']=[];state['phase']='sample';return
  if state['phase']=='shot':
   if t-state['at']<.3:return
   state['index']+=1;state['phase']='apply';return
  if state['phase']=='death':
   if t-state['at']<.3:return
   assert widget.get_active_danger_direction()==-1 and widget.get_render_opacity()==0
   assert all(a.get_visibility()==unreal.SlateVisibility.HIDDEN for a in arrows)
   report['death_hidden']=True;finish();return
  expected=cases[state['index']][2]
  pos=arrows[expected].get_editor_property('render_transform').translation
  if t-state['at']>.1:state['motion'].append(list(pos.to_tuple()))
  if t-state['at']<1:return
  visible=[i for i,a in enumerate(arrows) if a.get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE]
  assert visible==[expected],(cases[state['index']],visible,widget.get_active_danger_direction())
  assert widget.get_active_danger_direction()==expected
  axis=0 if expected in [1,3] else 1
  movement=max(p[axis] for p in state['motion'])-min(p[axis] for p in state['motion'])
  assert movement>4,(expected,movement)
  report['checks'].append(dict(case=cases[state['index']],visible=visible,movement=movement))
  if state['index']==0:
   unreal.SystemLibrary.execute_console_command(w,'Shot SHOWUI filename=C:/Dalssak/UE5/DKU-EndGame/Saved/DangerArrow/up.png');state['phase']='shot';state['at']=t;return
  state['index']+=1;state['phase']='apply'
 except Exception:finish(traceback.format_exc())
state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
