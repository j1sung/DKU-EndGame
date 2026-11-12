"""현재 판의 콜라이더/배치를 보존하며 얇은 겹면을 제거하고 무광 표면으로 정리한다."""
import json
import shutil
from pathlib import Path
import unreal

SURFACE_CODE = r'''
float2 cell = abs(frac((P.xy + 150.0) / 300.0) - .5) * 300.0;
float2 pixel = max(fwidth(P.xy), float2(.25,.25));
float2 stroke = 1.0 - smoothstep(float2(2,2), float2(2,2)+pixel, cell);
float fade = saturate(3.0 / max(pixel.x,pixel.y));
float seam = max(stroke.x,stroke.y) * fade;
float3 base = float3(.44,.49,.46) * (1.0 - .08*seam);
float diamondDistance = abs(abs(P.x)-730.0) + abs(abs(P.y)-430.0) - 55.0;
float width = max(fwidth(diamondDistance),1.0);
float mask = 1.0 - smoothstep(-width,width,diamondDistance);
float3 marking = P.x*P.y>0 ? float3(.78,.18,.075) : float3(.025,.34,.32);
return lerp(base,marking,mask*.7);
'''


def configure_material(material):
    lib=unreal.MaterialEditingLibrary
    world=lib.create_material_expression(material,unreal.MaterialExpressionWorldPosition)
    local=lib.create_material_expression(material,unreal.MaterialExpressionTransformPosition)
    local.set_editor_property('transform_source_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    inputs=lib.get_material_expression_input_names(local)
    assert inputs
    assert lib.connect_material_expressions(world,'',local,str(inputs[0]))
    surface=lib.create_material_expression(material,unreal.MaterialExpressionCustom)
    surface.set_editor_property('code',SURFACE_CODE)
    surface.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    p=unreal.CustomInput();p.set_editor_property('input_name','P');surface.set_editor_property('inputs',[p])
    assert lib.connect_material_expressions(local,'',surface,'P')
    assert lib.connect_material_property(surface,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,.92),(unreal.MaterialProperty.MP_SPECULAR,.2),(unreal.MaterialProperty.MP_METALLIC,0)]:
        e=lib.create_material_expression(material,unreal.MaterialExpressionConstant);e.set_editor_property('r',value)
        assert lib.connect_material_property(e,'',prop)
    lib.delete_unused_expressions(material)
    lib.recompile_material(material)


def apply():
    root=Path(unreal.Paths.project_dir());out=root/'Saved/Round4DeckSurface';out.mkdir(parents=True,exist_ok=True)
    base='/Game/LD_Gym/LDProps/StaticMeshes/Round4'
    mesh=unreal.load_asset(base+'/SM_R4A_FightingDeck_1800x1200');material=unreal.load_asset(base+'/Materials/M_R4A_Deck')
    assert mesh and material
    api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors=api.get_all_level_actors();transforms={a.get_path_name():a.get_actor_transform() for a in actors}
    rig=next(a for a in actors if a.get_actor_label()=='R4A_SeesawRig')
    settings={k:rig.get_editor_property(k) for k in ['maximum_tilt_angle','push_interval','initial_delay','tilt_travel_time','tilt_hold_time','boarding_travel_time']}
    body=mesh.get_editor_property('body_setup');collision=body.get_editor_property('agg_geom').export_text()
    trace=body.get_editor_property('collision_trace_flag')
    slots=[(str(s.material_slot_name),s.material_interface.get_path_name() if s.material_interface else '') for s in mesh.static_materials]
    nanite=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_nanite_settings(mesh).export_text()
    for asset,relative in [(mesh,'SM_R4A_FightingDeck_1800x1200.uasset'),(material,'Materials/M_R4A_Deck.uasset')]:
        if not (out/Path(relative).name).exists():
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
            shutil.copy2(root/'Content/LD_Gym/LDProps/StaticMeshes/Round4'/relative,out/Path(relative).name)
    dynamic=unreal.DynamicMesh()
    _,outcome=unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh,dynamic,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
    assert outcome==unreal.GeometryScriptOutcomePins.SUCCESS
    before=dynamic.get_triangle_count();remove=[]
    deck_index=None
    for triangle in range(dynamic.get_num_triangle_i_ds()):
        valid,a,b,c=dynamic.get_triangle_positions(triangle)
        if valid and max(abs(v.z-25) for v in [a,b,c])<.001 and abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))>1000000:
            deck_index=dynamic.get_triangle_material_id(triangle)[0];break
    assert deck_index is not None,'판 상면을 확인하지 못했습니다.'
    for triangle in range(dynamic.get_num_triangle_i_ds()):
        valid,a,b,c=dynamic.get_triangle_positions(triangle)
        if not valid:continue
        material_id,_=dynamic.get_triangle_material_id(triangle)
        if min(a.z,b.z,c.z)>=24.999 and max(a.z,b.z,c.z)<=25.11 and material_id!=deck_index:remove.append(triangle)
    assert len(remove)==104 or (len(remove)==0 and before==120),'표면 장식이 변경됐으므로 삭제 범위를 다시 확인해야 합니다: '+str(len(remove))
    with unreal.ScopedEditorTransaction('Clean Round4 deck surface'):
        mesh.modify();material.modify()
        for triangle in remove:
            _,deleted=dynamic.delete_triangle_from_mesh(triangle,True);assert deleted
        options=unreal.GeometryScriptCopyMeshToAssetOptions()
        _,outcome=unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dynamic,mesh,options,unreal.GeometryScriptMeshWriteLOD())
        assert outcome==unreal.GeometryScriptOutcomePins.SUCCESS
        configure_material(material)
    body=mesh.get_editor_property('body_setup')
    assert body.get_editor_property('agg_geom').export_text()==collision
    assert body.get_editor_property('collision_trace_flag')==trace
    assert [(str(s.material_slot_name),s.material_interface.get_path_name() if s.material_interface else '') for s in mesh.static_materials]==slots
    assert unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_nanite_settings(mesh).export_text()==nanite
    assert settings=={k:rig.get_editor_property(k) for k in settings}
    for a in api.get_all_level_actors():
        t=transforms[a.get_path_name()];now=a.get_actor_transform()
        assert now.translation.distance(t.translation)<.001 and now.scale3d.distance(t.scale3d)<.0001
        assert now.rotation.to_tuple()==t.rotation.to_tuple()
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    (out/'verified.json').write_text(json.dumps(dict(triangles_before=before,triangles_removed=len(remove),triangles_after=dynamic.get_triangle_count(),collision_preserved=collision,trace=str(trace),actor_count=len(actors),rig_settings=settings,roughness=.92,specular=.2),indent=2),encoding='utf-8')
    unreal.log('ROUND4_DECK_SURFACE_FIXED')


if __name__=='__main__':apply()
