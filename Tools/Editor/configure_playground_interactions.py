"""놀이터 맵의 오뚝이 회전 추종 및 항아리 물리 설정. 공유 원본 에셋은 수정하지 않는다."""
import unreal
import json
from pathlib import Path

MAP = '/Game/Map/L_LobbyJS_Playground'
SOURCE_MESH = '/Game/LD_Gym/LDProps/StaticMeshes/DummyProps/SM_Dummy_Drum_01'
PHYSICS_MESH = '/Game/Map/Playground/Meshes/SM_Playground_PushableDrum'
actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mesh_api = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0] == MAP, '놀이터 맵에서만 실행하세요.'
actors = {a.get_actor_label(): a for a in actor_api.get_all_level_actors()}
turn = actors['BP_PhysicsObstacle_TurnTable']
target = actors['BP_WobbleTarget2']
drums = [actors['SM_Dummy_Drum_01'], actors['SM_Dummy_Drum_2']]
turn_mesh = next(c for c in turn.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh)
dummy = next(c for c in target.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'Dummy')
constraint = target.get_component_by_class(unreal.PhysicsConstraintComponent)
assert dummy.is_simulating_physics()
assert target.get_actor_location().distance(turn.get_actor_location()) < 450, '회전판 위 타깃 위치 확인 필요'

# 물리 시뮬레이션은 삼각형 충돌만으로 동작하지 않으므로 전용 복제본에 볼록 충돌을 추가한다.
if unreal.EditorAssetLibrary.does_asset_exist(PHYSICS_MESH):
    mesh = unreal.load_asset(PHYSICS_MESH)
    assert mesh_api.get_convex_collision_count(mesh) > 0
else:
    mesh = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MESH, PHYSICS_MESH)
    assert mesh
    mesh_api.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.NDOP26)
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    assert mesh_api.get_convex_collision_count(mesh) > 0
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)

assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)

with unreal.ScopedEditorTransaction('Playground target follow and pushable drums'):
    target.modify()
    target.root_component.modify()
    target.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    target.attach_to_component(turn_mesh, '', unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    constraint.modify()
    # 회전판을 따라가는 로컬 BasePlate를 물리 기준으로 사용해 기존 흔들림을 유지한다.
    constraint.set_editor_property('constraint_actor1', None)
    constraint.set_editor_property('constraint_actor2', None)
    base = next(c for c in target.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'BasePlate')
    constraint.set_constrained_components(base, '', dummy, '')
    component1 = constraint.get_editor_property('component_name1')
    component1.set_editor_property('component_name', 'BasePlate')
    constraint.set_editor_property('component_name1', component1)
    constraint.set_disable_collision(True)
    for a in drums:
        a.modify()
        c = a.static_mesh_component
        c.modify()
        c.set_simulate_physics(False)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_static_mesh(mesh)
        c.set_collision_profile_name('PhysicsActor')
        c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        c.set_enable_gravity(True)
        c.set_mass_override_in_kg('', 12.0, True)
        c.set_linear_damping(0.4)
        c.set_angular_damping(0.8)
        c.set_use_ccd(True)
        c.set_simulate_physics(True)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)

# 저장 과정에서 컴포넌트가 재생성될 수 있으므로 다시 조회한다.
actors = {a.get_actor_label(): a for a in actor_api.get_all_level_actors()}
turn = actors['BP_PhysicsObstacle_TurnTable']
target = actors['BP_WobbleTarget2']
drums = [actors['SM_Dummy_Drum_01'], actors['SM_Dummy_Drum_2']]
turn_mesh = next(c for c in turn.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh)
dummy = next(c for c in target.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'Dummy')
constraint = target.get_component_by_class(unreal.PhysicsConstraintComponent)
report = {
 'map': world.get_path_name(),
 'target_parent': str(target.root_component.get_attach_parent()),
 'target_mobility': str(target.root_component.mobility),
 'constraint_actor1': str(constraint.get_editor_property('constraint_actor1')),
 'constraint_component1': constraint.get_editor_property('component_name1').export_text(),
 'constraint_actor2': str(constraint.get_editor_property('constraint_actor2')),
 'constraint_component2': constraint.get_editor_property('component_name2').export_text(),
 'target_still_simulates': dummy.is_simulating_physics(),
 'physics_mesh': mesh.get_path_name(),
 'convex_collision_count': mesh_api.get_convex_collision_count(mesh),
 'drums': [{'label':a.get_actor_label(),'simulates':a.static_mesh_component.is_simulating_physics(),'mass_kg':a.static_mesh_component.get_mass(),'profile':str(a.static_mesh_component.get_collision_profile_name())} for a in drums],
 'pie_tested': False
}
assert target.root_component.get_attach_parent() == turn_mesh
assert constraint.get_editor_property('constraint_actor1') is None
assert str(constraint.get_editor_property('component_name1').get_editor_property('component_name')) == 'BasePlate'
assert all(a.static_mesh_component.is_simulating_physics() for a in drums)
Path(unreal.Paths.project_saved_dir()+'PlaygroundWork/interactions-applied.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('PLAYGROUND_INTERACTIONS_APPLIED')




