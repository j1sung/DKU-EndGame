"""수평 및 양쪽 최대 기울기에서 네 벽의 Pawn 캡슐 차단을 검사한다."""
import json,time,traceback
from pathlib import Path
import unreal
OUT=Path(unreal.Paths.project_dir())/'Saved/Round4DeckBoundary';report={'samples':[],'tested':[]};state={'end':None};started=time.monotonic()
def finish(error=None):
 report['error']=error;unreal.EditorLevelLibrary.editor_end_play();state['end']=time.monotonic()
def tick(dt):
 try:
  w=unreal.EditorLevelLibrary.get_game_world()
  if state['end'] is not None:
   if w or time.monotonic()-state['end']<1:return
   unreal.unregister_slate_post_tick_callback(state['handle']);(OUT/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8');return
  if time.monotonic()-started>80:finish('PIE timeout');return
  if not w:return
  rig=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor) if a.get_actor_label()=='R4A_SeesawRig')
  deck=rig.get_editor_property('deck');angle=rig.get_editor_property('current_tilt_angle');t=unreal.GameplayStatics.get_time_seconds(w)
  stage='level' if abs(angle)<.01 else ('positive' if angle>38.8 else ('negative' if angle<-38.8 else None))
  if stage and stage not in report['tested']:
   boxes=[c for c in rig.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('DeckBoundary')];assert len(boxes)==4
   for c in boxes:assert c.get_attach_parent()==deck
   transform=deck.get_world_transform()
   for axis,side in [(0,-1),(0,1),(1,-1),(1,1)]:
    start=[0,0,350];end=[0,0,350];start[axis]=side*(700 if axis==0 else 400);end[axis]=side*(1150 if axis==0 else 850)
    p=unreal.MathLibrary.transform_location(transform,unreal.Vector(*start));q=unreal.MathLibrary.transform_location(transform,unreal.Vector(*end))
    hit=unreal.SystemLibrary.capsule_trace_single_by_profile(w,p,q,42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True)
    assert hit and hit.to_tuple()[0],(stage,axis,side,str(hit))
    values=hit.to_tuple();assert any(isinstance(v,unreal.BoxComponent) and v.get_name().startswith('DeckBoundary') for v in values),(stage,str(values))
    report['samples'].append(dict(t=t,angle=angle,axis=axis,side=side,hit=str(values)))
   report['tested'].append(stage)
  if len(report['tested'])==3:finish()
 except Exception:finish(traceback.format_exc())
state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.EditorLevelLibrary.editor_play_simulate()
