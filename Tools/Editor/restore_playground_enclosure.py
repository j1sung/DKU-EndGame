"""놀이터 맵의 외벽 높이와 2층 창문을 원본 로비 구성으로 복원한다."""
import unreal
import json
from pathlib import Path
MAP='/Game/Map/L_LobbyJS_Playground'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
# 원본 L_LobbyJS에서 조회한 액터 스케일. 기존 배치·회전·창문 메시·재질은 유지한다.
scales={
 'SM_Dummy_Wall_4x4':(1,10,1), 'SM_Dummy_Wall_4x5':(1,10,1),
 'SM_Dummy_Wall_4x6':(1,7.48,1), 'SM_Dummy_Wall_4x7':(1,7.49,1.010101),
 'SM_Dummy_Wall_4x8':(1,3,1), 'SM_Dummy_Wall_4x9':(1,7.49,1.68),
 'SM_Dummy_Wall_4x10':(1,7,1.68), 'SM_Dummy_Wall_4x11':(1,7.425,1.68),
 'SM_Dummy_Wall_4x12':(1,9.99,1.68),
 'SM_Dummy_Window_4x4':(1,1,1), 'SM_Dummy_Window_4x5':(1,1,1), 'SM_Dummy_Window_4x6':(1,1,1),
 'SM_WindowGlass':(2.1,5.94,1.1175), 'SM_WindowGlass2':(2.1,5.94,1.1175), 'SM_WindowGlass3':(2.1,5.94,1.1175)
}
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
assert all(k in actors for k in scales)
with unreal.ScopedEditorTransaction('Restore playground enclosure and upstairs windows'):
 for label,scale in scales.items():
  a=actors[label]
  a.modify()
  a.set_actor_scale3d(unreal.Vector(*scale))
  a.set_actor_hidden_in_game(False)
  a.set_is_temporarily_hidden_in_editor(False)
  a.set_actor_enable_collision(True)
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
actors={a.get_actor_label():a for a in api.get_all_level_actors()}
for label,expected in scales.items():
 a=actors[label]; s=a.get_actor_scale3d()
 assert max(abs(x-y) for x,y in zip((s.x,s.y,s.z),expected)) < 0.0001
 assert not a.get_editor_property('hidden')
 assert a.get_actor_enable_collision()
report={'map':MAP,'restored_wall_and_window_actors':len(scales),'window_frames':3,'glass_panels':3,'saved':True,'pie_tested':False}
Path(unreal.Paths.project_saved_dir()+'PlaygroundWork/enclosure-restored.json').write_text(json.dumps(report,indent=2))
unreal.log('PLAYGROUND_ENCLOSURE_RESTORED')

