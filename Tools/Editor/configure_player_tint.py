"""기존 흰색 머티리얼의 BaseColor를 보존하며 플레이어 Tint를 곱한다. 기본값은 조회."""
from datetime import datetime
from pathlib import Path
import shutil

MATERIAL = "/Game/Characters/WhiteRound/Mesh/M_White_Matte.M_White_Matte"
PARAMETER = "CF_PlayerTint"
MARKER = "CFPlayerTint_v1"
SOURCE_PARENT = "/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial.FBXLegacyPhongSurfaceMaterial"
TINT_PARENT = "/Game/Characters/WhiteRound/Mesh/M_CF_PlayerTintBase.M_CF_PlayerTintBase"


def instance_settings(asset):
    """구조체 메모리 주소 대신 직렬화된 값으로 저장 전후 설정을 비교한다."""
    values = {name: [item.export_text() for item in asset.get_editor_property(name)] for name in
              ("vector_parameter_values", "scalar_parameter_values", "texture_parameter_values")}
    values["base_property_overrides"] = asset.get_editor_property("base_property_overrides").export_text()
    return values


def make_plan(snapshot):
    if snapshot["path"] != MATERIAL or snapshot["dirty"] or snapshot["pie"]:
        raise ValueError("대상 머티리얼·미저장 변경·PIE 상태를 확인하세요.")
    if not snapshot["has_base"]:
        raise ValueError("BaseColor 연결이 없습니다. 기존 외형을 추측으로 변경하지 않습니다.")
    is_instance = snapshot.get("kind", "Material") == "MaterialInstanceConstant"
    if snapshot.get("kind", "Material") not in ("Material", "MaterialInstanceConstant"):
        raise ValueError("지원하지 않는 머티리얼 종류입니다.")
    if is_instance:
        if snapshot.get("parent") not in (SOURCE_PARENT, TINT_PARENT):
            raise ValueError("예상하지 못한 인스턴스 부모입니다. 임의로 교체하지 않습니다.")
        if snapshot["parent"] == SOURCE_PARENT and snapshot.get("destination_exists"):
            raise ValueError("프로젝트 Tint 부모 경로가 이미 있습니다. 덮어쓰지 않습니다.")
    if PARAMETER in snapshot["parameters"]:
        if snapshot["marker"] == MARKER and snapshot["base_marker"] == MARKER:
            return "unchanged"
        raise ValueError("같은 이름의 매개변수가 이미 있지만 이 도구가 만든 연결이 아닙니다.")
    if snapshot["marker"]:
        raise ValueError("이전 적용 표식과 실제 연결이 일치하지 않습니다.")
    if is_instance and snapshot["parent"] == TINT_PARENT:
        raise ValueError("관리 부모에 Tint 연결이 없습니다. 적용 상태를 확인하세요.")
    return "duplicate_parent" if is_instance else "multiply_base"


def run(apply=False):
    import unreal
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise ValueError("PIE를 종료하세요.")
    asset = unreal.load_asset(MATERIAL)
    if not isinstance(asset, (unreal.Material, unreal.MaterialInstanceConstant)):
        raise ValueError("예상한 Material 또는 MaterialInstanceConstant가 아닙니다.")
    is_instance = isinstance(asset, unreal.MaterialInstanceConstant)
    graph = asset.get_editor_property("parent") if is_instance else asset
    if not isinstance(graph, unreal.Material):
        raise ValueError("직접 부모가 Material이 아닙니다. 부모 체인을 임의 변경하지 않습니다.")
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    if asset.get_outer() in dirty or graph.get_outer() in dirty:
        raise ValueError("머티리얼의 미저장 변경을 먼저 확인하세요.")
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([graph])
    lib = unreal.MaterialEditingLibrary
    prop = unreal.MaterialProperty.MP_BASE_COLOR
    base = lib.get_material_property_input_node(graph, prop)
    snapshot = dict(path=asset.get_path_name(), dirty=False, pie=False, has_base=base is not None,
                    kind=asset.get_class().get_name(), parent=graph.get_path_name() if is_instance else None,
                    destination_exists=unreal.EditorAssetLibrary.does_asset_exist(TINT_PARENT),
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
    overrides = instance_settings(asset) if is_instance else {}
    if is_instance:
        # 공용 Interchange 부모는 보존하고, 인스턴스의 기존 파라미터가 유지되도록 부모 전체를 복제한다.
        graph = unreal.EditorAssetLibrary.duplicate_asset(graph.get_path_name(), TINT_PARENT.split(".")[0])
        if not isinstance(graph, unreal.Material):
            raise RuntimeError("프로젝트용 부모 복제 실패. 원본 인스턴스는 변경하지 않았습니다.")
        unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([graph])
        base = lib.get_material_property_input_node(graph, prop)
    output = lib.get_material_property_input_node_output_name(graph, prop)
    with unreal.ScopedEditorTransaction("CF 플레이어 색상 매개변수 연결"):
        graph.modify()
        tint = lib.create_material_expression(graph, unreal.MaterialExpressionVectorParameter, -450, 300)
        tint.set_editor_property("parameter_name", PARAMETER)
        tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
        multiply = lib.create_material_expression(graph, unreal.MaterialExpressionMultiply, -100, 0)
        multiply.set_editor_property("desc", MARKER)
        if not (lib.connect_material_expressions(base, output, multiply, "A") and
                lib.connect_material_expressions(tint, "RGB", multiply, "B") and
                lib.connect_material_property(multiply, "", prop)):
            raise RuntimeError("연결 실패: 저장하지 않았습니다. Undo 후 백업/에디터 상태를 확인하세요.")
        unreal.EditorAssetLibrary.set_metadata_tag(graph, MARKER, MARKER)
        errors = lib.recompile_material(graph)
        if errors:
            raise RuntimeError("Tint 부모 셰이더 컴파일 실패: " + str(errors))
        if is_instance:
            asset.modify()
            lib.set_material_instance_parent(asset, graph)
            lib.update_material_instance(asset)
        unreal.EditorAssetLibrary.set_metadata_tag(asset, MARKER, MARKER)
    if is_instance and instance_settings(asset) != overrides:
        raise RuntimeError("인스턴스 파라미터 보존 검사 실패. 저장하지 않았습니다.")
    if PARAMETER not in [str(name) for name in lib.get_vector_parameter_names(asset)]:
        raise RuntimeError("매개변수 확인 실패: 저장하지 않았습니다.")
    if not unreal.EditorAssetLibrary.save_loaded_asset(graph):
        raise RuntimeError("Tint 부모 저장 실패. 백업과 에디터 상태를 확인하세요.")
    if is_instance and not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("머티리얼 저장 실패. 에디터 상태와 백업을 확인하세요.")
    return run(apply=False)
