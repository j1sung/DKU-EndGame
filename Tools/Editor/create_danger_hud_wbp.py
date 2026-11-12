"""기존 C++ 부모를 사용하는 위험 효과 WBP를 최초 생성한다."""
import json
from pathlib import Path
import unreal
OUT=Path(unreal.Paths.project_dir())/'Saved/DangerHUD'
path='/Game/UI/HUD/WBP_CFDangerOverlay'
assert not unreal.EditorAssetLibrary.does_asset_exist(path)
f=unreal.WidgetBlueprintFactory();f.set_editor_property('parent_class',unreal.CFDangerOverlayWidget)
bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('WBP_CFDangerOverlay','/Game/UI/HUD',unreal.WidgetBlueprint,f);assert bp
assert unreal.CFDangerOverlayWidget.create_designer_content(bp,unreal.load_asset('/Game/UI/HUD/M_CFDangerVignette'))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
(OUT/'wbp_created.json').write_text(json.dumps(dict(asset=bp.get_path_name(),root='DangerCanvas',image='DangerVignette')),encoding='utf-8')
