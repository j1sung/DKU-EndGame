"""기존 범퍼의 움직임과 충돌을 보존하고 캐릭터 발사 컴포넌트를 추가한다."""
import json
import hashlib
import re
import shutil
from pathlib import Path

import unreal


ASSET = '/Game/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_SimpleObstacle_Bumper'


def run():
    project = Path(unreal.Paths.project_dir())
    work = project / 'Saved/BumperLaunch'
    work.mkdir(parents=True, exist_ok=True)
    bp = unreal.load_asset(ASSET)
    assert bp
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    fn = unreal.SubobjectDataBlueprintFunctionLibrary

    def components():
        return [fn.get_object(fn.get_data(h)) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]

    def snapshot():
        data = {}
        for c in components():
            if isinstance(c, unreal.SceneComponent):
                entry = {p: str(c.get_editor_property(p)) for p in (
                    'relative_location', 'relative_rotation', 'relative_scale3d')}
                if isinstance(c, unreal.PrimitiveComponent):
                    entry['collision'] = str(c.get_collision_enabled())
                    entry['profile'] = str(c.get_collision_profile_name())
                if isinstance(c, unreal.StaticMeshComponent):
                    entry['mesh'] = c.static_mesh.get_path_name() if c.static_mesh else None
                data[c.get_name()] = entry
        cdo = unreal.get_default_object(bp.generated_class())
        data['defaults'] = {p: str(cdo.get_editor_property(p)) for p in (
            'Duration', 'Location', 'Rotation', 'Loop', 'AutoStart', 'StartDelay', 'InterpBehavior')}
        # Blueprint 컴파일로 재생성되는 Python 래퍼의 메모리 주소는 비교에서 제외한다.
        return json.loads(re.sub(r'0x[0-9A-Fa-f]+', 'PTR', json.dumps(data)))

    cls = unreal.load_class(None, '/Script/DKUEndGame.BumperCharacterLaunchComponent')
    assert cls, '새 Editor 빌드가 필요합니다.'
    assert not any(c and c.get_class() == cls for c in components()), '이미 적용된 범퍼입니다.'
    before = snapshot()
    backup = work / 'BP_SimpleObstacle_Bumper_before.uasset'
    source = project / 'Content/LD_Gym/Levels/LevelPrototyping/Obstacles/BP_SimpleObstacle_Bumper.uasset'
    if backup.exists():
        assert hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(backup.read_bytes()).digest(), '백업과 다른 원본입니다.'
    else:
        shutil.copy2(source, backup)
    with unreal.ScopedEditorTransaction('Add bumper character launch'):
        handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=sub.k2_gather_subobject_data_for_blueprint(bp)[0],
            new_class=cls, blueprint_context=bp))
        component = fn.get_object(fn.get_data(handle))
        assert component, str(reason)
        assert sub.rename_subobject(handle, 'CharacterLauncher')
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        after = snapshot()
        (work / 'comparison.json').write_text(json.dumps({'before': before, 'after': after}, indent=2), encoding='utf-8')
        assert after == before, '범퍼의 기존 설정이 바뀌었습니다.'
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
    (work / 'applied.json').write_text(json.dumps({
        'asset': ASSET, 'component': 'CharacterLauncher', 'launch_speed': 900,
        'preserved': before}, indent=2), encoding='utf-8')
    unreal.log('BUMPER_CHARACTER_LAUNCH_APPLIED')


if __name__ == '__main__':
    run()
