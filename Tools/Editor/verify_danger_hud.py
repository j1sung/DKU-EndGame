"""임시 PIE에서 로컬 HUD의 각도/경고 폭/탈락 시 숨김을 확인한다. 맵은 저장하지 않는다."""
import json,time,traceback
from pathlib import Path
import unreal
if getattr(unreal,'_cf_danger_test_handle',None):
 try:unreal.unregister_slate_post_tick_callback(unreal._cf_danger_test_handle)
 except Exception:pass
OUT=Path(unreal.Paths.project_dir())/'Saved/DangerHUD';report={'checks':[]};state={'end':None,'index':0,'at':0};started=time.monotonic()
# 각도, 경고 폭, 예상 강도. 대각선은 우세 축 기준이어야 한다.
cases=[(0,0,10,0),(49,0,10,0),(50,0,10,0),(55,0,10,.5),(59,0,10,.9),(60,0,10,1),(45,45,10,0),(50,0,20,.5)]
def finish(error=None):
 report['error']=error;unreal.EditorLevelLibrary.editor_end_play();state['end']=time.monotonic()
def tick(dt):
 try:
  w=unreal.EditorLevelLibrary.get_game_world()
  if state['end'] is not None:
   if w or time.monotonic()-state['end']<1:return
   unreal.unregister_slate_post_tick_callback(state['handle']);unreal._cf_danger_test_handle=None;(OUT/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8');return
  if time.monotonic()-started>30:finish('PIE timeout');return
  if not w:return
  pc=unreal.GameplayStatics.get_player_controller(w,0)
  if not pc:return
  pawn=pc.get_controlled_pawn()
  if not isinstance(pawn,unreal.ChickenCharacter):
   pawn=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.ChickenCharacter))
   pc.possess(pawn)
  pawn.set_actor_tick_enabled(False)
  pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
  pres=pawn.get_component_by_class(unreal.CFPlayerPresentationComponent)
  widgets=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.CFDangerOverlayWidget,False)
  if not widgets:return
  visible=[a for a in widgets if a.is_in_viewport()];assert len(visible)==1
  widget=visible[0];assert widget.get_owning_player()==pc
  assert widget.get_class().get_path_name()=='/Game/UI/HUD/WBP_CFDangerOverlay.WBP_CFDangerOverlay_C'
  if 'initial_margin' not in report:report['initial_margin']=pres.get_editor_property('danger_warning_angle_margin')
  report['widget_class']=widget.get_class().get_path_name()
  if 'limit' not in report:report['limit']=pawn.get_balance_failure_angle_degrees()
  t=unreal.GameplayStatics.get_time_seconds(w)
  if t-state['at']<.75:return
  i=state['index']
  if state.get('pending'):
   expected=state['pending'][3];actual=widget.get_render_opacity()
   assert abs(actual-expected)<.002,(state['pending'],actual)
   report['checks'].append(dict(case=state['pending'],opacity=actual,widget_count=len(visible)))
   if i==6:
    unreal.SystemLibrary.execute_console_command(w,'Shot SHOWUI filename=C:/Dalssak/UE5/DKU-EndGame/Saved/DangerHUD/full.png')
   state['pending']=None
   if i==6:
    state['at']=t
    return
  if i<len(cases):
   x,y,margin,expected=cases[i]
   pawn.set_editor_property('body_tilt',unreal.Vector2D(x/35,y/35));pres.set_editor_property('danger_warning_angle_margin',margin)
   result=pawn.get_balance_danger_intensity(margin);assert abs(result-expected)<.002,(i,result)
   state['pending']=[x,y,margin,expected];state['index']+=1;state['at']=t
  elif i==len(cases):
   pawn.start_knockdown(unreal.CFKnockdownDirection.FORWARD)
   assert pawn.get_balance_danger_intensity(10)==0
   state['pending']=[0,0,10,0];state['index']+=1;state['at']=t
  else:
   assert widget.get_render_opacity()==0
   report['death_hidden']=True;finish()
 except Exception:finish(traceback.format_exc())
state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal._cf_danger_test_handle=state['handle']
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
