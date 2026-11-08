"""Round3 2층을 높이고 회전 막대와 재생성 통나무를 적용한다. 기존 배치에 한 번만 적용."""
import unreal,math,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir());WORK=ROOT/'Saved/Round3Revision';WORK.mkdir(parents=True,exist_ok=True)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();BASE='/Game/Map/Round3Draft'
assert world.get_path_name().split('.')[0]=='/Game/Map/L_Round3'
actors=api.get_all_level_actors();assert not any(a.get_actor_label().startswith('R3M_') for a in actors),'이미 적용됨'
log_parent=unreal.load_class(None,'/Script/DKUEndGame.Round3LogSpawner');assert log_parent,'Round3LogSpawner 빌드 필요'
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Map/L_Round3')
shutil.copy2(ROOT/'Content/Map/L_Round3.umap',WORK/('before_apply_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap'))
by={a.get_actor_label():a for a in actors};saved_character=by['BP_ChickenCharacter'].get_actor_location()
# 기존 형상은 유지하고 높은 층과 접근로만 450→600cm로 올린다.
for a in actors:
 label=a.get_actor_label();p=a.get_actor_location();s=a.get_actor_scale3d()
 if label.startswith('R3L_Upper_Support'):
  a.set_actor_location(unreal.Vector(p.x,p.y,280),False,False);a.set_actor_scale3d(unreal.Vector(s.x,s.y,5.6))
 elif label.startswith('R3L_Upper_') or label=='R3L_BridgeMotion':a.set_actor_location(p+unreal.Vector(0,0,150),False,False)
 elif label.startswith('R3L_Stairs_'):
  a.set_actor_location(unreal.Vector(p.x,p.y,p.z*4/3),False,False);a.set_actor_scale3d(unreal.Vector(s.x,s.y,s.z*4/3))
 elif label in ['R3L_Ramp_West','R3L_Ramp_East']:
  sign=-1 if 'West' in label else 1;d=unreal.Vector(0,-sign*750,600)
  a.set_actor_location(unreal.Vector(p.x,p.y,280),False,False);a.set_actor_rotation(unreal.MathLibrary.make_rot_from_x(d),False);a.set_actor_scale3d(unreal.Vector((d.length()+12)/100,4.2,.4))
# 높은 다리 위에 배치된 테스트 캐릭터도 해당 보행면으로 옮긴다. XY와 설정은 유지한다.
if abs(saved_character.x)<900 and abs(saved_character.y)<130 and 380<saved_character.z<600:
 z=600-45*math.cos(math.pi*saved_character.x/1800)+90
 by['BP_ChickenCharacter'].set_actor_location(unreal.Vector(saved_character.x,saved_character.y,z),False,False)
# 이전 회전판은 중앙으로 교체하며 기존 엄폐물은 비어 있던 북동쪽에 남긴다.
api.destroy_actor(by['R3A_RotatingFloor'])
for a,(x,y) in zip([a for a in actors if a.get_actor_label()=='R3A_Arena_CenterCover'],[(550,1050),(850,650)]):a.set_actor_location(unreal.Vector(x,y,45),False,False)
f=unreal.BlueprintFactory();f.set_editor_property('parent_class',unreal.StaticMeshActor)
bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_R3_Sweeper',BASE,unreal.Blueprint,f)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);fn=unreal.SubobjectDataBlueprintFunctionLibrary
h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=sub.k2_gather_subobject_data_for_blueprint(bp)[0],new_class=unreal.RotatingMovementComponent,blueprint_context=bp));c=fn.get_object(fn.get_data(h));assert c,str(reason);c.set_editor_property('rotation_rate',unreal.Rotator(yaw=30))
unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
rotor=api.spawn_actor_from_class(bp.generated_class(),unreal.Vector(0,0,130));rotor.set_actor_label('R3M_Sweeper');rotor.set_folder_path('Round3Layers/Sweeper');rotor.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);rotor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cylinder'));rotor.static_mesh_component.set_collision_profile_name('BlockAllDynamic');rotor.static_mesh_component.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE);rotor.set_actor_scale3d(unreal.Vector(9,9,.2));rotor.static_mesh_component.set_material(0,unreal.load_asset(BASE+'/Materials/M_R3_Balance'))
# 막대와 중심축도 개별 액터로 편집할 수 있게 부착한다.
for label,z,size,mesh in [('Bar',210,(800,45,45),'Cube'),('Hub',200,(120,120,120),'Cylinder')]:
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,z));a.set_actor_label('R3M_Sweeper_'+label);a.set_folder_path('Round3Layers/Sweeper');c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+mesh));c.set_collision_profile_name('BlockAllDynamic');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA,unreal.CollisionResponseType.ECR_IGNORE);c.set_material(0,unreal.load_asset(BASE+'/Materials/M_R3_Moving'));c.set_absolute(False,False,True);assert a.attach_to_component(rotor.root_component,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False);a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
f=unreal.BlueprintFactory();f.set_editor_property('parent_class',log_parent);bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_R3_LogSpawner',BASE,unreal.Blueprint,f);unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
for side,x,y in [('West',-1250,-850),('East',1250,850)]:
 old=by['R3L_RollingLog_'+side];mesh=old.static_mesh_component.static_mesh;mat=old.static_mesh_component.get_material(0);api.destroy_actor(old)
 a=api.spawn_actor_from_class(bp.generated_class(),unreal.Vector(x,y,532),unreal.Rotator(pitch=90));a.set_actor_label('R3M_LogSpawner_'+side);a.set_folder_path('Round3Layers/Logs');a.set_actor_scale3d(unreal.Vector(.9,.9,3.2));c=a.get_component_by_class(unreal.StaticMeshComponent);c.set_static_mesh(mesh);c.set_material(0,mat)
 a.set_editor_property('respawn_delay',2.);a.set_editor_property('finish_height',65.);a.set_editor_property('maximum_lifetime',12.)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Map/L_Round3')
(WORK/'applied.json').write_text(json.dumps({'actors':len(api.get_all_level_actors()),'upper_height':600,'respawn_delay':2,'maximum_lifetime':12,'finish_height':65,'sweeper_diameter':900,'bar_length':800,'yaw_per_second':30},indent=2));unreal.log('R3_REVISION_APPLIED')

