"""Round4 최초 생성 뒤 축 위의 판 높이, 출입 동선, 전용 하늘을 마무리한다."""
import json
from pathlib import Path
import unreal

BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round4';MAP='/Game/Map/L_Round4_Amusement'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
assert not any(a.get_actor_label()=='R4A_SkyDome' for a in api.get_all_level_actors()),'최초 마무리 도구입니다.'
actors=list(api.get_all_level_actors());rig=next(a for a in actors if a.get_actor_label()=='R4A_SeesawRig')
# 이전 생성 상태가 남아 있는 첫 맵만 새 네이티브 기본 구조로 재생성한다.
if rig.get_editor_property('deck').get_editor_property('relative_location').z<900:
    cls=rig.get_class();api.destroy_actor(rig);rig=api.spawn_actor_from_class(cls,unreal.Vector())
    rig.set_actor_label('R4A_SeesawRig');rig.set_folder_path('Round4Amusement/Ride')
for a in actors:
    if not unreal.SystemLibrary.is_valid(a):continue
    label=a.get_actor_label();p=a.get_actor_location()
    if label.startswith('R4A_Block_Cream') and abs(abs(p.x)-1230)<1 and abs(p.y)<1:
        a.set_actor_location(unreal.Vector(p.x,p.y,487.5),False,False);a.set_actor_scale3d(unreal.Vector(2.6,2.4,9.75))
    elif label.startswith('R4A_StairFlight_875'):a.set_actor_scale3d(unreal.Vector(1,1,975/875))
    elif label.startswith('R4A_PusherHousing'):a.set_actor_location(unreal.Vector(-480 if p.x<0 else 480,p.y,p.z),False,False)
    elif label.startswith('R4A_FencePanel_240') and str(a.get_folder_path())=='Round4Amusement/Boarding':
        a.set_actor_location(unreal.Vector(p.x,p.y,975),False,False)
    elif label.startswith('R4A_ArrowMark') and abs(abs(p.x)-1230)<1 and p.y>-1400:
        a.set_actor_location(unreal.Vector(p.x,p.y,975*(p.y+1370)/1250+1),False,False)
    elif label.startswith(('R4A_PlayerStart_','R4A_PreviewPlayer_')):a.set_actor_location(unreal.Vector(p.x,p.y,1070),False,False)
    elif label.startswith('R4A_ConcessionBooth') and abs(p.x)>2100:
        a.set_actor_rotation(unreal.Rotator(yaw=90 if p.x<0 else -90),False)

tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
sky_mesh=unreal.EditorAssetLibrary.duplicate_asset('/Engine/BasicShapes/Sphere',BASE+'/SM_R4A_SkySphere');assert sky_mesh
m=tools.create_asset('M_R4A_Sky',BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
m.set_editor_property('two_sided',True);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
p=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition)
e=lib.create_material_expression(m,unreal.MaterialExpressionCustom);e.set_editor_property('code','return lerp(float3(.62,.77,.94),float3(.12,.37,.78),saturate(P.z/35000));')
e.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);i=unreal.CustomInput();i.set_editor_property('input_name','P');e.set_editor_property('inputs',[i]);lib.connect_material_expressions(p,'',e,'P');lib.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m);sky_mesh.set_material(0,m)
a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector());a.set_actor_label('R4A_SkyDome');a.set_folder_path('Round4Amusement/Lighting')
a.set_actor_scale3d(unreal.Vector(1000,1000,1000));c=a.static_mesh_component;c.set_static_mesh(sky_mesh);c.set_collision_profile_name('NoCollision');c.set_cast_shadow(False)
unreal.EditorAssetLibrary.save_loaded_asset(m);unreal.EditorAssetLibrary.save_loaded_asset(sky_mesh)
# 엔진 공유 에셋을 수정하지 않고 환경 큐브도 전용 폴더에 보관한다.
cube=unreal.EditorAssetLibrary.duplicate_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap',BASE+'/Textures/T_R4A_AmbientSky');assert cube
for a in api.get_all_level_actors():
    if a.get_actor_label()=='R4A_SkyLight':a.light_component.set_editor_property('cubemap',cube)
unreal.EditorAssetLibrary.save_loaded_asset(cube)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
root=Path(unreal.Paths.project_dir())
(root/'Saved/Round4Amusement/polished.json').write_text(json.dumps(dict(deck_top_z=975,pivot_z=850,actors=len(api.get_all_level_actors()),assets=list(unreal.EditorAssetLibrary.list_assets(BASE,True,False))),indent=2),encoding='utf-8')
unreal.log('ROUND4_AMUSEMENT_POLISHED')

