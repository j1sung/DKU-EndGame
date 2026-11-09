"""네 라운드의 경기 GameMode/미리보기 표식을 연결한다. 기본값은 조회만 한다."""
import hashlib
import json
import runpy
import shutil
from datetime import datetime
from pathlib import Path

ROUND_MODE = "/Script/DKUEndGame.CFRoundGameMode"
TEST_MODE = "/Game/Testing/Knockdown/BP_CF_KnockdownTestGameMode.BP_CF_KnockdownTestGameMode_C"
ARENA_MODE = "/Game/UI/WaitingRoom/BP_CF_ArenaGameMode.BP_CF_ArenaGameMode_C"
PREVIEW_TAG = "CFRoundSpawnPreview"


def make_plan(snapshot, previews):
    if snapshot["game_mode"] not in (None, TEST_MODE, ARENA_MODE, ROUND_MODE):
        raise ValueError("예상하지 못한 GameMode입니다. 사용자 설정을 덮어쓰지 않습니다.")
    markers = {m["name"] for m in snapshot["markers"]}
    if len(markers) != 4 or len(previews) != 4 or {p["name"] for p in previews} != markers:
        raise ValueError("확인한 배치 캐릭터 4개가 모두 필요합니다.")
    tagged_starts = [s for s in snapshot["starts"] if "arenaspawn" in [str(t).casefold() for t in s["tags"]]]
    if len(tagged_starts) != 4:
        raise ValueError("ArenaSpawn 4개를 먼저 연결해야 합니다.")
    if any(p["attached"] or p["controlled"] or p.get("owned", False) for p in previews):
        raise ValueError("부착 관계 또는 소유 컨트롤러가 있는 캐릭터는 미리보기로 지정하지 않습니다.")
    tagged = [p for p in previews if PREVIEW_TAG.casefold() in [str(t).casefold() for t in p["tags"]]]
    if len(tagged) not in (0, 4):
        raise ValueError("미리보기 표식이 일부만 있습니다. 적용 상태를 확인하세요.")
    return {"game_mode": ROUND_MODE, "preview_names": sorted(markers), "net_load_on_client": False}


def _capture(unreal, spawns, world, actors, map_path):
    source = spawns["_snapshot"](unreal, world, actors, map_path)
    spawns["_verify"](unreal, actors, source["plan"])
    names = {m["name"] for m in source["markers"]}
    previews, poses = [], {}
    for actor in actors:
        p, r, s = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
        poses[actor.get_name()] = [p.x, p.y, p.z, r.pitch, r.yaw, r.roll, s.x, s.y, s.z]
        tags = [str(tag) for tag in actor.get_editor_property("tags")]
        if spawns["has_tag"](tags, PREVIEW_TAG) and actor.get_name() not in names:
            raise RuntimeError("다른 액터에 미리보기 표식이 있습니다. 임의로 정리하지 않습니다.")
        if actor.get_name() in names:
            previews.append({"name": actor.get_name(), "tags": tags,
                             "controlled": bool(actor.get_controller()),
                             "owned": bool(actor.get_owner()),
                             "net_load_on_client": bool(actor.get_editor_property("net_load_on_client")),
                             "attached": bool(actor.get_attach_parent_actor() or actor.get_attached_actors())})
    previews.sort(key=lambda p: p["name"])
    return {"source": source, "previews": previews, "poses": poses, "plan": make_plan(source, previews)}


def _verify(after, before):
    if after["source"]["game_mode"] != ROUND_MODE:
        raise RuntimeError("경기 GameMode 적용을 확인하지 못했습니다.")
    if (after["poses"] != before["poses"] or
            after["source"]["markers"] != before["source"]["markers"] or
            after["source"]["starts"] != before["source"]["starts"]):
        raise RuntimeError("배치/스폰 보존 검사 실패. 맵을 저장하거나 추가 적용하지 않습니다.")
    for current, original in zip(after["previews"], before["previews"]):
        expected = {t.casefold() for t in original["tags"]} | {PREVIEW_TAG.casefold()}
        if {t.casefold() for t in current["tags"]} != expected:
            raise RuntimeError("기존 캐릭터 태그 보존 검사 실패.")
        if current["net_load_on_client"]:
            raise RuntimeError("미리보기 캐릭터의 네트워크 클라이언트 로드를 차단하지 못했습니다.")


def run(apply=False):
    import unreal
    root = Path(unreal.Paths.project_dir()).resolve()
    if not (root / "DKUEndGame.uproject").is_file():
        raise RuntimeError("DKUEndGame 프로젝트가 아닙니다.")
    spawns = runpy.run_path(str(root / "Tools/Editor/link_round_spawns.py"))
    level_api = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if level_api.is_in_play_in_editor():
        raise RuntimeError("Play/Simulate를 종료하세요.")
    spawns["_clean"](unreal)
    target = unreal.load_class(None, ROUND_MODE)
    if not target:
        raise RuntimeError("최신 프로젝트 Editor 빌드가 필요합니다. CFRoundGameMode를 찾지 못했습니다.")
    defaults = unreal.get_default_object(target)
    for property_name, expected in (
        ("default_pawn_class", "/Game/Characters/WhiteRound/Blueprints/BP_CF_WaitingCharacter.BP_CF_WaitingCharacter_C"),
        ("player_controller_class", "/Game/UI/WaitingRoom/BP_CF_ArenaController.BP_CF_ArenaController_C"),
    ):
        value = defaults.get_editor_property(property_name)
        if not value or value.get_path_name() != expected:
            raise RuntimeError("기존 대기 캐릭터/컨트롤러 Blueprint 연결이 올바르지 않습니다.")
    files = [root / "Content/Map" / (path.rsplit("/", 1)[1] + ".umap") for path in spawns["MAPS"]]
    for path in files:
        if not path.is_file():
            raise RuntimeError("라운드 맵 파일이 없습니다.")
        if any(folder.exists() for folder in (root / "Content/__ExternalActors__/Map" / path.stem,
                                              root / "Content/Map/__ExternalActors__" / path.stem)):
            raise RuntimeError("외부 액터 맵은 자동 적용 대상이 아닙니다.")
    hashes = [hashlib.sha256(path.read_bytes()).hexdigest() for path in files]
    reports = []
    for map_path in spawns["MAPS"]:
        world, actors = spawns["_load"](unreal, level_api, actor_api, map_path)
        state = _capture(unreal, spawns, world, actors, map_path)
        reports.append(state)
        unreal.log("CF_ROUND_GAMEPLAY_AUDIT " + json.dumps({"map": map_path, "before_game_mode": state["source"]["game_mode"], "plan": state["plan"]}, ensure_ascii=False))
        del world, actors
    spawns["_clean"](unreal)
    if not apply:
        unreal.log("CF_ROUND_GAMEPLAY: 네 맵 조회 완료. 에셋은 변경하지 않았습니다.")
        return reports
    if hashes != [hashlib.sha256(path.read_bytes()).hexdigest() for path in files]:
        raise RuntimeError("조회 중 맵이 바뀌었습니다. 다시 조회하세요.")
    backup = root / "Saved/RoundGameplaySetup" / datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    backup.mkdir(parents=True, exist_ok=False)
    for path in files:
        shutil.copy2(path, backup / path.name)
    (backup / "before.json").write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("CF_ROUND_GAMEPLAY_BACKUP " + str(backup))
    for index, before in enumerate(reports):
        if hashlib.sha256(files[index].read_bytes()).hexdigest() != hashes[index]:
            raise RuntimeError("적용 직전 맵이 바뀌었습니다. 남은 맵은 처리하지 않습니다.")
        map_path = before["source"]["map"]
        world, actors = spawns["_load"](unreal, level_api, actor_api, map_path)
        if _capture(unreal, spawns, world, actors, map_path) != before:
            raise RuntimeError("적용 전 맵 상태가 달라졌습니다.")
        names = set(before["plan"]["preview_names"])
        with unreal.ScopedEditorTransaction("Configure four-player round gameplay"):
            settings = world.get_world_settings()
            settings.modify()
            settings.set_editor_property("default_game_mode", target)
            for actor in actors:
                if actor.get_name() in names:
                    actor.modify()
                    tags = list(actor.get_editor_property("tags"))
                    if not spawns["has_tag"](tags, PREVIEW_TAG):
                        tags.append(unreal.Name(PREVIEW_TAG))
                        actor.set_editor_property("tags", tags)
                    actor.set_editor_property("net_load_on_client", False)
            _verify(_capture(unreal, spawns, world, actors, map_path), before)
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
            raise RuntimeError("맵 저장 실패. 백업과 에디터 변경을 확인하세요.")
        del world, actors, actor, settings
        world, actors = spawns["_load"](unreal, level_api, actor_api, map_path)
        _verify(_capture(unreal, spawns, world, actors, map_path), before)
        unreal.log("CF_ROUND_GAMEPLAY_SAVED " + map_path)
        del world, actors
    unreal.log("CF_ROUND_GAMEPLAY: 네 맵 저장/재로딩 완료. 실제 4인 생성과 EOS 입장 검증은 별도입니다.")
    return reports


if __name__ == "__main__":
    run()
