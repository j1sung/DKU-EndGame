"""기존 이동 컴포넌트를 보존하며 발판 BP에 끝점 대기 컴포넌트를 추가한다."""
import json
import shutil
from pathlib import Path

import unreal


ASSET = '/Game/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_PhysicsObstacle_MovingPlatform'


def run():
    project = Path(unreal.Paths.project_dir())
    work = project / 'Saved/MovingPlatformPause'
    work.mkdir(parents=True, exist_ok=True)
    bp = unreal.load_asset(ASSET)
    assert bp, ASSET
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    fn = unreal.SubobjectDataBlueprintFunctionLibrary

    def components():
        return [fn.get_object(fn.get_data(h)) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]

    def movement_state():
        movers = [c for c in components() if isinstance(c, unreal.InterpToMovementComponent)]
        assert len(movers) == 1, '이동 컴포넌트를 하나로 특정할 수 없습니다.'
        c = movers[0]
        return {name: str(c.get_editor_property(name)) for name in (
            'duration', 'control_points', 'behaviour_type', 'sweep', 'auto_activate', 'speed_multiplier')}

    before = movement_state()
    cls = unreal.load_class(None, '/Script/DKUEndGame.MovingPlatformEndpointPauseComponent')
    assert cls, '새 Editor 빌드를 먼저 실행하세요.'
    existing = [c for c in components() if c and c.get_class() == cls]
    assert not existing, 'EndpointPause가 이미 있어 재적용하지 않습니다.'
    source = project / 'Content/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_PhysicsObstacle_MovingPlatform.uasset'
    backup = work / 'BP_PhysicsObstacle_MovingPlatform_before.uasset'
    assert not backup.exists(), '기존 백업을 덮어쓰지 않습니다.'
    shutil.copy2(source, backup)
    with unreal.ScopedEditorTransaction('Add moving platform endpoint pause'):
        handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=sub.k2_gather_subobject_data_for_blueprint(bp)[0],
            new_class=cls, blueprint_context=bp))
        component = fn.get_object(fn.get_data(handle))
        assert component, str(reason)
        assert sub.rename_subobject(handle, 'EndpointPause')
        component.set_editor_property('top_wait_seconds', 0.0)
        component.set_editor_property('bottom_wait_seconds', 0.0)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert movement_state() == before, '기존 이동 설정이 변경되었습니다.'
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    (work / 'applied.json').write_text(json.dumps({
        'asset': ASSET, 'movement_preserved': before,
        'top_wait_seconds': 0, 'bottom_wait_seconds': 0,
        'component': 'EndpointPause'}, indent=2), encoding='utf-8')
    unreal.log('MOVING_PLATFORM_PAUSE_APPLIED')


if __name__ == '__main__':
    run()
