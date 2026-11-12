"""Round3 원판을 고정하고 기존 회전 부모는 막대·중심축만 회전시킨다."""
from datetime import datetime
from pathlib import Path
import shutil
import unreal

MAP='/Game/Map/L_Round3'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]==MAP
by={a.get_actor_label():a for a in api.get_all_level_actors()}
assert 'R3M_Sweeper_Floor' not in by,'이미 바닥을 분리했습니다.'
rotor=by['R3M_Sweeper'];source=rotor.get_component_by_class(unreal.StaticMeshComponent)
assert source.static_mesh,'회전 원판 메시를 찾을 수 없습니다.'
assert rotor.get_component_by_class(unreal.RotatingMovementComponent)
root=Path(unreal.Paths.project_dir());work=root/'Saved/Round3Theme'
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
shutil.copy2(root/'Content/Map/L_Round3.umap',work/('before_fixed_floor_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap'))

with unreal.ScopedEditorTransaction('Fix Round3 sweeper floor rotation'):
    floor=api.spawn_actor_from_class(unreal.StaticMeshActor,rotor.get_actor_location(),rotor.get_actor_rotation())
    floor.set_actor_label('R3M_Sweeper_Floor');floor.set_folder_path('Round3Layers/Sweeper')
    c=floor.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_static_mesh(source.static_mesh)
    for i in range(source.get_num_materials()):c.set_material(i,source.get_material(i))
    c.set_editor_property('use_default_collision',False)
    c.set_collision_profile_name(source.get_collision_profile_name())
    c.set_collision_object_type(source.get_collision_object_type())
    for name in ['ECC_WORLD_STATIC','ECC_WORLD_DYNAMIC','ECC_PAWN','ECC_VISIBILITY','ECC_CAMERA','ECC_PHYSICS_BODY','ECC_VEHICLE','ECC_DESTRUCTIBLE']:
        channel=getattr(unreal.CollisionChannel,name)
        c.set_collision_response_to_channel(channel,source.get_collision_response_to_channel(channel))
    c.set_collision_enabled(source.get_collision_enabled())
    c.set_cast_shadow(source.get_editor_property('cast_shadow'))
    # 위치만 부모를 따라가고, 매 프레임 회전과 비균일 스케일은 상속하지 않는다.
    c.set_absolute(False,True,True)
    assert floor.attach_to_component(rotor.root_component,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
    floor.set_actor_scale3d(rotor.get_actor_scale3d())
    rotor.modify();source.modify();source.set_static_mesh(None)
    source.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
unreal.log('R3_SWEEPER_FIXED_FLOOR_SAVED')
