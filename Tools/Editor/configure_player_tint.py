"""기존 흰색 머티리얼의 BaseColor를 보존하며 플레이어 Tint를 곱한다. 기본값은 조회."""
from datetime import datetime
from pathlib import Path
import shutil

MATERIAL = "/Game/Characters/WhiteRound/Mesh/M_White_Matte.M_White_Matte"
PARAMETER = "CF_PlayerTint"
MARKER = "CFPlayerTint_v1"


def make_plan(snapshot):
    if snapshot["path"] != MATERIAL or snapshot["dirty"] or snapshot["pie"]:
        raise ValueError("대상 머티리얼·미저장 변경·PIE 상태를 확인하세요.")
    if not snapshot["has_base"]:
        raise ValueError("BaseColor 연결이 없습니다. 기존 외형을 추측으로 변경하지 않습니다.")
    if PARAMETER in snapshot["parameters"]:
        if snapshot["marker"] == MARKER and snapshot["base_marker"] == MARKER:
            return "unchanged"
        raise ValueError("같은 이름의 매개변수가 이미 있지만 이 도구가 만든 연결이 아닙니다.")
    if snapshot["marker"]:
        raise ValueError("이전 적용 표식과 실제 연결이 일치하지 않습니다.")
    return "multiply_base"


def run(apply=False):
    import unreal
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise ValueError("PIE를 종료하세요.")
    asset = unreal.load_asset(MATERIAL)
    if not isinstance(asset, unreal.Material):
        raise ValueError("예상한 원본 Material이 아닙니다.")
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    if asset.get_outermost() in dirty:
        raise ValueError("머티리얼의 미저장 변경을 먼저 확인하세요.")
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])
    lib = unreal.MaterialEditingLibrary
    prop = unreal.MaterialProperty.MP_BASE_COLOR
    base = lib.get_material_property_input_node(asset, prop)
    snapshot = dict(path=asset.get_path_name(), dirty=False, pie=False, has_base=base is not None,
                    parameters=[str(name) for name in lib.get_vector_parameter_names(asset)],
                    marker=unreal.EditorAssetLibrary.get_metadata_tag(asset, MARKER),
                    base_marker=str(base.get_editor_property("desc")) if base else "")
    action = make_plan(snapshot)
    unreal.log("CF 플레이어 Tint: " + action + (" (조회)" if not apply else " (적용)"))
    if not apply or action == "unchanged":
        return snapshot
    source = Path(unreal.Paths.project_content_dir()).resolve() / "Characters/WhiteRound/Mesh/M_White_Matte.uasset"
    backup = Path(unreal.Paths.project_saved_dir()).resolve() / "PlayerTintSetup" / datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    backup.mkdir(parents=True, exist_ok=False)
    shutil.copy2(source, backup / source.name)
    unreal.log("원본 백업: " + str(backup))
    output = lib.get_material_property_input_node_output_name(asset, prop)
    with unreal.ScopedEditorTransaction("CF 플레이어 색상 매개변수 연결"):
        asset.modify()
        tint = lib.create_material_expression(asset, unreal.MaterialExpressionVectorParameter, -450, 300)
        tint.set_editor_property("parameter_name", PARAMETER)
        tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
        multiply = lib.create_material_expression(asset, unreal.MaterialExpressionMultiply, -100, 0)
        multiply.set_editor_property("desc", MARKER)
        if not (lib.connect_material_expressions(base, output, multiply, "A") and
                lib.connect_material_expressions(tint, "RGB", multiply, "B") and
                lib.connect_material_property(multiply, "", prop)):
            raise RuntimeError("연결 실패: 저장하지 않았습니다. Undo 후 백업/에디터 상태를 확인하세요.")
        unreal.EditorAssetLibrary.set_metadata_tag(asset, MARKER, MARKER)
        lib.recompile_material(asset)
    if PARAMETER not in [str(name) for name in lib.get_vector_parameter_names(asset)]:
        raise RuntimeError("매개변수 확인 실패: 저장하지 않았습니다.")
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("머티리얼 저장 실패. 에디터 상태와 백업을 확인하세요.")
    return run(apply=False)
