"""시소 Blueprint의 FightingDeck에 네 방향 투명 충돌 상자를 붙인다."""
import json,math,shutil
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Round4DeckBoundary';MAP='/Game/Map/L_Round4_Amusement'
BP='/Game/LD_Gym/LDProps/StaticMeshes/Round4/Blueprints/BP_R4A_SeesawRig'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
before=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
bp=unreal.load_asset(BP);sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);fn=unreal.SubobjectDataBlueprintFunctionLibrary
handles=sub.k2_gather_subobject_data_for_blueprint(bp)
assert not any(fn.get_object(fn.get_data(h)).get_name().startswith('DeckBoundary') for h in handles),'이미 적용된 시소입니다.'
parent=next(h for h in handles if fn.get_object(fn.get_data(h)).get_name()=='FightingDeck')
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
shutil.copy2(ROOT/'Content/Map/L_Round4_Amusement.umap',OUT/'before_boundary.umap')
shutil.copy2(ROOT/'Content/LD_Gym/LDProps/StaticMeshes/Round4/Blueprints/BP_R4A_SeesawRig.uasset',OUT/'before_boundary.uasset')
# 안쪽 면을 판 끝보다 10cm 안으로 겹치고, 모서리와 바닥에도 여유를 둔다.
specs=[('DeckBoundary_NegX',(-910,0,300),(20,630,325)),('DeckBoundary_PosX',(910,0,300),(20,630,325)),('DeckBoundary_NegY',(0,-610,300),(930,20,325)),('DeckBoundary_PosY',(0,610,300),(930,20,325))]
with unreal.ScopedEditorTransaction('Add seesaw deck invisible boundaries'):
 for name,loc,extent in specs:
    h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.BoxComponent,blueprint_context=bp))
    c=fn.get_object(fn.get_data(h));assert c,str(reason);assert sub.rename_subobject(h,name)
    c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('relative_location',unreal.Vector(*loc));c.set_box_extent(unreal.Vector(*extent),False)
    c.set_collision_profile_name('Custom');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS);c.set_collision_object_type(unreal.CollisionChannel.ECC_WORLD_DYNAMIC)
    c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE);c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN,unreal.CollisionResponseType.ECR_BLOCK)
    c.set_editor_property('generate_overlap_events',False);c.set_editor_property('can_character_step_up_on',unreal.CanBeCharacterBase.ECB_NO)
    c.set_editor_property('cast_shadow',False);c.set_hidden_in_game(True);c.set_visibility(False,False)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
after={a.get_path_name():a for a in api.get_all_level_actors()}
assert len(after)==len(before['actors'])
for row in before['actors']:
 a=after[row['path']]
 for method,key in [(a.get_actor_location,'loc'),(a.get_actor_rotation,'rot'),(a.get_actor_scale3d,'scale')]:assert math.dist(method().to_tuple(),row[key])<.001
rig=next(a for a in after.values() if a.get_actor_label()=='R4A_SeesawRig')
assert before['rig']=={k:rig.get_editor_property(k) for k in before['rig']}
boxes=[c for c in rig.get_components_by_class(unreal.BoxComponent) if c.get_name().startswith('DeckBoundary')];assert len(boxes)==4
for c in boxes:
 assert c.get_attach_parent()==rig.get_editor_property('deck')
 assert c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)==unreal.CollisionResponseType.ECR_BLOCK
 assert c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA)==unreal.CollisionResponseType.ECR_IGNORE
 assert c.get_editor_property('hidden_in_game') and not c.get_editor_property('cast_shadow')
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(OUT/'applied.json').write_text(json.dumps(dict(actors_preserved=len(after),rig=before['rig'],boxes=[dict(name=c.get_name(),location=list(c.get_editor_property('relative_location').to_tuple()),extent=list(c.get_unscaled_box_extent().to_tuple())) for c in boxes]),indent=2),encoding='utf-8')
unreal.log('ROUND4_DECK_BOUNDARIES_APPLIED')
