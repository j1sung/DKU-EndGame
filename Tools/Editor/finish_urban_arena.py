import unreal,json
from pathlib import Path
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();MAP='/Game/Map/L_TestJS';BASE='/Game/Map/UrbanArena'
assert world.get_path_name().split('.')[0]==MAP
actors=api.get_all_level_actors();by={a.get_actor_label():a for a in actors}
assert 'UA_BackdropGround' not in by,'이미 마감된 레벨입니다.'
with unreal.ScopedEditorTransaction('Finish urban plaza orientation and surrounding blocks'):
 for label,yaw in [('Cube36',90),('Cube37',-90),('Cube38',0),('Cube39',180)]:
  a=by[label];a.modify();a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
 for i,a in enumerate(actors):
  if a.get_actor_label().startswith('UA_'):
   a.modify();r=a.get_actor_rotation();a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=r.pitch if abs(r.pitch)>.01 else r.yaw,roll=0),False);a.set_actor_label(a.get_actor_label()+'_%02d'%i)
 # A second block provides a city backdrop through gaps in the existing perimeter.
 points=[(-5100,y) for y in [-3200,-1300,700,2700,4700]]+[(5050,y) for y in [-3200,-1000,1200,3400,5500]]+[(x,5700) for x in [-3100,-900,1300,3300]]+[(x,-4900) for x in [-3100,-900,1300,3300]]
 for i,(x,y) in enumerate(points):
  m=unreal.load_asset(BASE+'/Meshes/SM_Urban_Building_%02d'%([1,2,3,6][i%4]));a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,-30),unreal.Rotator(pitch=0,yaw=(i%4)*90,roll=0));a.set_actor_label('UA_Backdrop_%02d'%i);a.set_folder_path('UrbanArena/Backdrop');a.static_mesh_component.set_static_mesh(m);a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_scale3d(unreal.Vector(1.2,1.15,.72+(i%5)*.16))
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(50,530,-32));a.set_actor_label('UA_BackdropGround');a.set_folder_path('UrbanArena/Backdrop');a.static_mesh_component.set_static_mesh(unreal.load_asset(BASE+'/Meshes/SM_Urban_PlazaPaving'));a.set_actor_scale3d(unreal.Vector(2.9,2.9,1));a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
 # Save after selecting the intended editor view so reopening retains the plaza framing.
 unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-1800,-1900,2200),unreal.Rotator(pitch=-25,yaw=54,roll=0))
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
unreal.log('URBAN_ARENA_FINISHED')
