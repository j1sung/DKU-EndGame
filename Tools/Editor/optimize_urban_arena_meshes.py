"""생성 도심 메시의 재질별 면 묶음·Nanite를 적용한다. UrbanArena 소유 에셋만 갱신."""
import unreal,json
from pathlib import Path
BASE='/Game/Map/UrbanArena';ROOT=Path(unreal.Paths.project_dir());SRC=ROOT/'Saved/UrbanArenaWork/Source';data=json.loads((SRC/'manifest.json').read_text());sys=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]=='/Game/Map/L_TestJS'
mats={k:unreal.load_asset(BASE+'/Materials/M_Urban_'+k) for k in list(data['palette'])+data['signs']};assert all(mats.values())
tasks=[]
for item in data['meshes']:
 t=unreal.AssetImportTask();t.filename=str(SRC/(item['name']+'.obj'));t.destination_path=BASE+'/Meshes';t.destination_name=item['name'];t.automated=True;t.replace_existing=True;t.save=False
 opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.combine_meshes=True;t.options=opt;tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for item in data['meshes']:
 m=unreal.load_asset(BASE+'/Meshes/'+item['name']);assert m
 for i,slot in enumerate(m.static_materials):m.set_material(i,mats[str(slot.material_slot_name)])
 if item.get('collision'):sys.remove_collisions(m);sys.add_simple_collisions(m,unreal.ScriptingCollisionShapeType.BOX)
 if item['name']=='SM_Urban_Grandstand':m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 if item.get('replace') or item['name']=='SM_Urban_PlazaPaving':
  settings=sys.get_nanite_settings(m);settings.set_editor_property('enabled',True);sys.set_nanite_settings(m,settings,True)
 unreal.EditorAssetLibrary.save_loaded_asset(m,False)
unreal.log('URBAN_MESHES_OPTIMIZED')
