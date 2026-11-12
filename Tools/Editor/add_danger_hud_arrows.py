"""기존 위험 WBP의 화면 구성은 보존하고 방향 화살표 네 개만 추가한다."""
import json,re
from pathlib import Path
import unreal
OUT=Path(unreal.Paths.project_dir())/'Saved/DangerArrow';OUT.mkdir(parents=True,exist_ok=True)
BASE='/Game/UI/HUD';bp=unreal.load_asset(BASE+'/WBP_CFDangerOverlay');assert bp
tr=unreal.find_object(bp,'WidgetTree');vignette=unreal.find_object(tr,'DangerVignette');assert vignette
def style():return {p:re.sub(r'0x[0-9A-Fa-f]+','PTR',str(vignette.get_editor_property(p))) for p in ['brush','color_and_opacity','render_transform','visibility']}
before=style()
assert not any(unreal.find_object(tr,n) for n in ['DangerArrowUp','DangerArrowRight','DangerArrowDown','DangerArrowLeft'])
name='M_CFDangerArrow';assert not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/'+name)
lib=unreal.MaterialEditingLibrary;mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE,unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
e=lib.create_material_expression(mat,unreal.MaterialExpressionCustom);e.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
i=unreal.CustomInput();i.set_editor_property('input_name','UV');e.set_editor_property('inputs',[i]);e.set_editor_property('code','''float2 p=(UV-0.5)*2.0;
float2 a=float2(0,-0.65), b=float2(-0.45,-0.12), c=float2(0.45,-0.12), z=float2(0,0.7);
float2 v=b-a; float d=length(p-a-v*saturate(dot(p-a,v)/dot(v,v)));
v=c-a; d=min(d,length(p-a-v*saturate(dot(p-a,v)/dot(v,v))));
v=z-a; d=min(d,length(p-a-v*saturate(dot(p-a,v)/dot(v,v))));
float aa=max(fwidth(d),0.003);
float alpha=1.0-smoothstep(0.11-aa,0.11+aa,d);
float white=1.0-smoothstep(0.055-aa,0.055+aa,d);
return float4(lerp(float3(0.015,0.015,0.015),float3(1,1,1),white),alpha);''')
lib.connect_material_expressions(uv,'',e,'UV')
vertex=lib.create_material_expression(mat,unreal.MaterialExpressionVertexColor)
for channel,prop in [('RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('A',unreal.MaterialProperty.MP_OPACITY)]:
 mask=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
 for field in ['r','g','b','a']:mask.set_editor_property(field,(field!='a') if channel=='RGB' else field=='a')
 assert lib.connect_material_expressions(e,'',mask,'')
 mul=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply);lib.connect_material_expressions(mask,'',mul,'A');assert lib.connect_material_expressions(vertex,'' if channel=='RGB' else 'A',mul,'B');lib.connect_material_property(mul,'',prop)
lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
assert unreal.CFDangerOverlayWidget.add_designer_arrows(bp,mat)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
assert before==style(),'기존 비네트 디자인이 바뀌었습니다.'
(OUT/'applied.json').write_text(json.dumps(dict(asset=bp.get_path_name(),vignette_preserved=before,material=mat.get_path_name()),indent=2),encoding='utf-8')
