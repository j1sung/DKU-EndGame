"""댐 테마 적용 후 물 전용 셰이딩·젖은 가장자리·암벽 기단을 다듬는다."""
import json
from pathlib import Path
import unreal

BASE='/Game/LD_Gym/LDProps/StaticMeshes/Round3'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]=='/Game/Map/L_Round3'
assert not getattr(unreal,'_round2_capture_running',False)
lib=unreal.MaterialEditingLibrary


def vector(m,rgb):
    e=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    e.set_editor_property('constant',unreal.LinearColor(*rgb,1));return e


m=unreal.load_asset(BASE+'/Materials/M_R3D_Water');assert m
out=lib.create_material_expression(m,unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
names=lib.get_material_expression_input_names(out)
unreal.log('R3_WATER_INPUTS '+str(names))
for name,rgb in zip(names[:2],[(.003,.014,.012),(.09,.022,.018)]):
    assert lib.connect_material_expressions(vector(m,rgb),'',out,name)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
opacity=lib.create_material_expression(m,unreal.MaterialExpressionConstant);opacity.set_editor_property('r',.12)
lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
lib.connect_material_property(vector(m,(.015,.08,.07)),'',unreal.MaterialProperty.MP_BASE_COLOR)
n=lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_NORMAL)
n.set_editor_property('code','float a=P.x*.075+P.y*.027+T*1.1; float b=P.y*.095-P.x*.035-T*.8; return normalize(float3(cos(a)*.12+sin(b)*.06,sin(a)*.1+cos(b)*.12,1));')
lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)

# 바닥과 벽 하단의 습기/이끼 얼룩. 카메라 거리에 따른 고주파 무늬는 줄인다.
for key in ['Concrete','Coping']:
    m=unreal.load_asset(BASE+'/Materials/M_R3D_'+key)
    e=lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
    color='float3(.57,.55,.46)' if key=='Concrete' else 'float3(.68,.65,.55)'
    e.set_editor_property('code','''
float n=sin(dot(P,float3(.127,.231,.173)))*sin(dot(P,float3(.71,.13,.37)));
float broad=sin(P.x*.009+sin(P.y*.012))*sin(P.z*.018+P.y*.007);
float edge=max(smoothstep(1390,1580,abs(P.x)),smoothstep(1710,1930,abs(P.y+45)));
float damp=edge*(1-smoothstep(25,160,P.z))*smoothstep(-.3,.6,broad)*.48;
return lerp(COLOR+n*.006+broad*.025,float3(.16,.20,.10),damp);
'''.replace('COLOR',color))
    lib.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)


def prop(label,mesh,loc,scale,rock=False,yaw=0):
    if any(a.get_actor_label()==label for a in api.get_all_level_actors()):return
    a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(yaw=yaw));a.set_actor_label(label);a.set_folder_path('Round3Dam/Cliffs')
    c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset(BASE+'/'+mesh));c.set_collision_profile_name('NoCollision');c.set_editor_property('use_default_collision',False)
    if rock:c.set_material(0,unreal.load_asset(BASE+'/Materials/M_R3D_Rock'))
    a.set_actor_scale3d(unreal.Vector(*scale))


for side in [-1,1]:
    prop('R3D_CanyonFoot_X_'+str(side),'SM_R3D_Cliff_0',(side*2150,-45,-1070),(1.375,6,1.17),True)
    prop('R3D_CanyonFoot_Y_'+str(side),'SM_R3D_Cliff_1',(0,side*2500,-1070),(6.5,1.25,1.17),True)
    for i in range(8):
        prop('R3D_LowRock_'+str(side)+'_'+str(i),'SM_R3D_Cliff_'+str(i%3),(side*1950,-2100+i*600,-500),(.72,.8,.68),yaw=i*41)

for a in api.get_all_level_actors():
    if a.get_actor_label().startswith('R3D_Puddle_'):
        p=a.get_actor_location();a.set_actor_location(unreal.Vector(p.x,p.y,1.2),False,False)
if not any(a.get_actor_label()=='R3D_WaterReflection' for a in api.get_all_level_actors()):
    a=api.spawn_actor_from_class(unreal.SphereReflectionCapture,unreal.Vector(0,300,260))
    a.set_actor_label('R3D_WaterReflection');a.set_folder_path('Round3Dam/Water')
    a.get_component_by_class(unreal.SphereReflectionCaptureComponent).set_editor_property('influence_radius',5000.)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Map/L_Round3')
unreal.log('R3_DAM_POLISH_DONE')
