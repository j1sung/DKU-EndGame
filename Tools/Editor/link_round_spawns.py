"""배치된 닭 캐릭터 4곳을 ArenaSpawn에 연결한다. 기본 실행은 조회만 한다."""

import hashlib
import json
import math
import shutil
from datetime import datetime
from pathlib import Path

MAPS = tuple(f"/Game/Map/L_Round{i}" for i in range(1, 5))
ARENA_TAG = "ArenaSpawn"
MANAGED_TAG = "CFLinkedRoundSpawn"


def has_tag(tags, wanted):
    return wanted.casefold() in [str(tag).casefold() for tag in tags]


def make_plan(markers, starts):
    """Unreal과 독립된 대상 선정/캡슐 간 겹침 검사. 위치는 자동 보정하지 않는다."""
    if len(markers) != 4 or len({m["name"] for m in markers}) != 4:
        raise ValueError("시작 위치용 캐릭터가 정확히 4개여야 합니다.")
    if len({s["name"] for s in starts}) != len(starts):
        raise ValueError("PlayerStart 이름이 중복됩니다. 임의로 선택하지 않습니다.")
    markers = sorted(markers, key=lambda m: m["name"])
    for marker in markers:
        values = marker["location"] + marker["rotation"] + [marker["radius"], marker["half_height"]]
        if not all(math.isfinite(v) for v in values) or marker["radius"] <= 0 or marker["half_height"] < marker["radius"]:
            raise ValueError("캐릭터 위치/회전/캡슐 값이 올바르지 않습니다.")
    for index, a in enumerate(markers):
        for b in markers[index + 1:]:
            dx, dy, dz = (a["location"][i] - b["location"][i] for i in range(3))
            segment_gap = max(0, abs(dz) - (a["half_height"] - a["radius"]) - (b["half_height"] - b["radius"]))
            if math.sqrt(dx * dx + dy * dy + segment_gap * segment_gap) <= a["radius"] + b["radius"]:
                raise ValueError("시작 위치용 캐릭터 캡슐이 겹칩니다. 에디터에서 위치를 확인하세요.")
    tagged = [s for s in starts if has_tag(s["tags"], ARENA_TAG)]
    managed = [s for s in starts if has_tag(s["tags"], MANAGED_TAG)]
    if len(tagged) not in (0, 4) or len(managed) not in (0, 4):
        raise ValueError("기존 ArenaSpawn 또는 도구 관리 스폰이 일부만 있습니다. 임의로 덮어쓰지 않습니다.")
    if managed and tagged and {s["name"] for s in managed} != {s["name"] for s in tagged}:
        raise ValueError("기존 스폰과 도구 관리 스폰이 서로 다릅니다.")
    selected = managed or tagged or (starts if len(starts) == 4 else [])
    if any(s["class"] != "/Script/Engine.PlayerStart" for s in selected):
        raise ValueError("사용자 정의 PlayerStart는 자동 변경하지 않습니다.")
    if any(s.get("attached", False) for s in selected):
        raise ValueError("부착 부모/자식이 있는 PlayerStart는 다른 액터에 영향을 줄 수 있어 자동 이동하지 않습니다.")
    selected = sorted(selected, key=lambda s: s["name"])
    return [{"marker": marker, "start": selected[i]["name"] if selected else None}
            for i, marker in enumerate(markers)]


def _clean(unreal):
    dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
             list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    if dirty:
        raise RuntimeError("저장하지 않은 맵/에셋이 있습니다. 직접 저장 또는 취소한 뒤 다시 실행하세요.")


def _snapshot(unreal, world, actors, map_path):
    chicken = unreal.load_class(None, "/Script/DKUEndGame.ChickenCharacter")
    if not chicken:
        raise RuntimeError("프로젝트 Editor 빌드가 필요합니다. ChickenCharacter 클래스를 찾지 못했습니다.")
    markers, starts = [], []
    for actor in actors:
        if isinstance(actor, unreal.Character):
            if not actor.get_path_name().startswith(map_path + "."):
                raise RuntimeError("서브레벨 캐릭터를 발견했습니다. 현재 도구는 단일 레벨만 처리합니다.")
            if not unreal.MathLibrary.class_is_child_of(actor.get_class(), chicken):
                raise RuntimeError("닭 캐릭터가 아닌 Character가 있습니다. 시작 위치를 임의 선택하지 않습니다.")
            p, r, s = actor.get_actor_location(), actor.get_actor_rotation(), actor.get_actor_scale3d()
            capsule = actor.get_component_by_class(unreal.CapsuleComponent)
            markers.append({"name": actor.get_name(), "class": actor.get_class().get_path_name(),
                            "location": [p.x, p.y, p.z], "rotation": [r.pitch, r.yaw, r.roll],
                            "scale": [s.x, s.y, s.z], "radius": capsule.get_scaled_capsule_radius(),
                            "half_height": capsule.get_scaled_capsule_half_height()})
        elif isinstance(actor, unreal.PlayerStart):
            if not actor.get_path_name().startswith(map_path + "."):
                raise RuntimeError("서브레벨 PlayerStart는 자동 변경하지 않습니다.")
            starts.append({"name": actor.get_name(), "class": actor.get_class().get_path_name(),
                           "tags": [str(tag) for tag in actor.get_editor_property("tags")],
                           "attached": bool(actor.get_attach_parent_actor() or actor.get_attached_actors())})
    mode = world.get_world_settings().get_editor_property("default_game_mode")
    return {"map": map_path, "game_mode": mode.get_path_name() if mode else None,
            "markers": sorted(markers, key=lambda m: m["name"]), "starts": sorted(starts, key=lambda s: s["name"]),
            "plan": make_plan(markers, starts)}


def _load(unreal, level_api, actor_api, map_path):
    _clean(unreal)
    if not level_api.load_level(map_path):
        raise RuntimeError(f"맵 로드 실패: {map_path}")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split(".")[0] != map_path:
        raise RuntimeError(f"다른 맵이 열렸습니다: {map_path}")
    return world, list(actor_api.get_all_level_actors())


def _verify(unreal, actors, plan):
    tagged = [a for a in actors if isinstance(a, unreal.PlayerStart) and has_tag(a.get_editor_property("tags"), ARENA_TAG)]
    if len(tagged) != 4:
        raise RuntimeError("ArenaSpawn 4개 검증에 실패했습니다.")
    unmatched = list(tagged)
    for row in plan:
        marker = row["marker"]
        for actor in unmatched:
            p, r = actor.get_actor_location(), actor.get_actor_rotation()
            position_ok = all(abs(a - b) < .01 for a, b in zip((p.x, p.y, p.z), marker["location"]))
            rotation_ok = all(abs((a - b + 180) % 360 - 180) < .01 for a, b in zip((r.pitch, r.yaw, r.roll), marker["rotation"]))
            if position_ok and rotation_ok:
                unmatched.remove(actor)
                break
        else:
            raise RuntimeError("스폰 지점과 원본 캐릭터 위치/회전이 일치하지 않습니다.")


def run(apply=False):
    """Unreal Editor Python에서 실행. apply=True일 때만 백업·변경·저장한다."""
    import unreal
    level_api = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if level_api.is_in_play_in_editor():
        raise RuntimeError("Play/Simulate를 종료한 뒤 실행하세요.")
    _clean(unreal)
    root = Path(unreal.Paths.project_dir()).resolve()
    files = [root / "Content" / "Map" / (path.rsplit("/", 1)[1] + ".umap") for path in MAPS]
    if not (root / "DKUEndGame.uproject").is_file() or not all(path.is_file() for path in files):
        raise RuntimeError("DKUEndGame 프로젝트와 라운드 맵 4개를 확인하세요.")
    for path in files:
        # 외부 액터 패키지는 .umap 백업만으로 보호할 수 없으므로 이 도구에서 제외한다.
        external = (root / "Content" / "__ExternalActors__" / "Map" / path.stem,
                    root / "Content" / "Map" / "__ExternalActors__" / path.stem)
        if any(folder.exists() for folder in external):
            raise RuntimeError("외부 액터 맵은 자동 적용 대상이 아닙니다.")
    hashes = [hashlib.sha256(path.read_bytes()).hexdigest() for path in files]
    reports = []
    for map_path in MAPS:
        world, actors = _load(unreal, level_api, actor_api, map_path)
        report = _snapshot(unreal, world, actors, map_path)
        reports.append(report)
        unreal.log("CF_ROUND_SPAWN_AUDIT " + json.dumps(report, ensure_ascii=False))
        del world, actors
    _clean(unreal)
    if not apply:
        unreal.log("CF_ROUND_SPAWN: 조회 완료. 맵을 변경하거나 저장하지 않았습니다.")
        return reports
    if hashes != [hashlib.sha256(path.read_bytes()).hexdigest() for path in files]:
        raise RuntimeError("조회 중 맵 파일이 바뀌었습니다. 다시 조회하세요.")
    backup = root / "Saved" / "RoundSpawnLink" / datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    backup.mkdir(parents=True, exist_ok=False)
    for path in files:
        shutil.copy2(path, backup / path.name)
    (backup / "before.json").write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("CF_ROUND_SPAWN_BACKUP " + str(backup))
    for map_index, report in enumerate(reports):
        if hashlib.sha256(files[map_index].read_bytes()).hexdigest() != hashes[map_index]:
            raise RuntimeError("적용 직전 맵 파일이 바뀌었습니다. 남은 맵은 적용하지 않습니다.")
        world, actors = _load(unreal, level_api, actor_api, report["map"])
        before = _snapshot(unreal, world, actors, report["map"])
        if before != report:
            raise RuntimeError("적용 전 맵 상태가 바뀌었습니다. 남은 맵은 적용하지 않습니다.")
        by_name = {actor.get_name(): actor for actor in actors}
        with unreal.ScopedEditorTransaction("Link four round spawn positions"):
            for index, row in enumerate(report["plan"], 1):
                marker = row["marker"]
                position = unreal.Vector(*marker["location"])
                rotation = unreal.Rotator(pitch=marker["rotation"][0], yaw=marker["rotation"][1], roll=marker["rotation"][2])
                start = by_name[row["start"]] if row["start"] else actor_api.spawn_actor_from_class(unreal.PlayerStart, position, rotation)
                if not start:
                    raise RuntimeError("PlayerStart 생성 실패. 맵을 저장하지 않았습니다.")
                start.modify()
                start.set_actor_location_and_rotation(position, rotation, False, True)
                tags = list(start.get_editor_property("tags"))
                for tag in (ARENA_TAG, MANAGED_TAG):
                    if not has_tag(tags, tag):
                        tags.append(unreal.Name(tag))
                start.set_editor_property("tags", tags)
                if row["start"] is None:
                    start.set_actor_label(f"CF_RoundSpawn_{index:02d}")
                    start.set_folder_path("RoundSpawns")
            actors = list(actor_api.get_all_level_actors())
            _verify(unreal, actors, report["plan"])
            after = _snapshot(unreal, world, actors, report["map"])
            if after["markers"] != before["markers"] or after["game_mode"] != before["game_mode"]:
                raise RuntimeError("캐릭터 또는 GameMode 보존 검사 실패. 맵을 저장하지 않았습니다.")
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, report["map"]):
            raise RuntimeError("맵 저장 실패. 백업과 변경 상태를 확인하세요.")
        del world, actors, by_name, start
        world, actors = _load(unreal, level_api, actor_api, report["map"])
        _verify(unreal, actors, report["plan"])
        reloaded = _snapshot(unreal, world, actors, report["map"])
        if reloaded["markers"] != report["markers"] or reloaded["game_mode"] != report["game_mode"]:
            raise RuntimeError("재로딩 후 캐릭터/GameMode가 달라졌습니다. 백업과 결과를 확인하세요.")
        unreal.log("CF_ROUND_SPAWN_SAVED " + report["map"])
        del world, actors
    unreal.log("CF_ROUND_SPAWN: 네 맵의 스폰 저장·재로딩 확인 완료. 실제 충돌/4인 플레이 검증은 별도입니다.")
    return reports


if __name__ == "__main__":
    run()
