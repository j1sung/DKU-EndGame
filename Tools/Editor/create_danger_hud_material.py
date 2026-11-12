"""균형 위험 HUD용 UI 재질을 최초 생성한다. 기존 재질은 덮어쓰지 않는다."""
import unreal
base='/Game/UI/HUD';name='M_CFDangerVignette';assert not unreal.EditorAssetLibrary.does_asset_exist(base+'/'+name)
mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,base,unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
lib=unreal.MaterialEditingLibrary
uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
mask=lib.create_material_expression(mat,unreal.MaterialExpressionCustom);mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
inp=unreal.CustomInput();inp.set_editor_property('input_name','UV');mask.set_editor_property('inputs',[inp]);mask.set_editor_property('code','float r=length((UV-0.5)*2.0); float t=saturate((r-0.35)/0.85); return t*t*(3.0-2.0*t)*0.72;')
lib.connect_material_expressions(uv,'',mask,'UV');lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY)
c=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);c.set_editor_property('constant',unreal.LinearColor(.9,.005,.002,1));lib.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('DANGER_VIGNETTE_MATERIAL_CREATED')
