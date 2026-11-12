"""Round3의 두 통나무에만 고속 접촉 밀침과 280kg 질량을 적용한다."""
from datetime import datetime
from pathlib import Path
import shutil
import unreal

MAP='/Game/Map/L_Round3'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
logs=[a for a in api.get_all_level_actors() if a.get_actor_label() in ['R3M_LogSpawner_West','R3M_LogSpawner_East']]
assert len(logs)==2
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
root=Path(unreal.Paths.project_dir());out=root/'Saved/Round3LogPush';out.mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'Content/Map/L_Round3.umap',out/('before_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap'))
with unreal.ScopedEditorTransaction('Round3 rolling logs push players'):
    for a in logs:
        a.modify();a.set_editor_property('enable_rolling_player_push',True)
        a.set_editor_property('log_mass',280.)
        assert a.get_editor_property('enable_rolling_player_push')
        assert a.get_editor_property('log_mass')==280.
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
unreal.log('R3_ROLLING_LOG_PUSH_ENABLED')
