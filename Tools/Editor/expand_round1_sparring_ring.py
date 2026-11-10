"""L_Round1 중앙 링을 12m→16m로 확대한다. 기존 12m 배치에 한 번만 적용."""
import unreal,json,shutil,math
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Round1Expand';OUT.mkdir(parents=True,exist_ok=True);MAP='/Game/Map/L_Round1'
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().split('.')[0]==MAP
actors=api.get_all_level_actors();by={a.get_actor_label():a for a in actors};floor=by['Floor_Arena_12m4'];o,e=floor.get_actor_bounds(False);assert abs(e.x*2-1200)<.1 and abs(e.y*2-1200)<.1,'12m 링 배치에만 최초 적용'
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
backup=OUT/('L_Round1_before_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.umap');shutil.copy2(ROOT/'Content/Map/L_Round1.umap',backup)
before={a.get_name():{'loc':a.get_actor_location(),'scale':a.get_actor_scale3d(),'rot':a.get_actor_rotation()} for a in actors};changed=[];cx,cy=o.x,o.y

def move(a,x,y):
 p=a.get_actor_location();a.modify();a.set_actor_location(unreal.Vector(x,y,p.z),False,False);changed.append(a.get_name())
def shift(a,x,y):p=a.get_actor_location();move(a,p.x+x,p.y+y)
def scale_xy(a,factor=None,add=None):
 a.modify();s=a.get_actor_scale3d()
 if add is not None:
  _,extent=a.get_actor_bounds(False);fx=(extent.x*2+add)/(extent.x*2);fy=(extent.y*2+add)/(extent.y*2)
 else:fx=fy=factor
 a.set_actor_scale3d(unreal.Vector(s.x*fx,s.y*fy,s.z));changed.append(a.get_name())
with unreal.ScopedEditorTransaction('Expand Round1 sparring ring from 12m to 16m'):
 scale_xy(floor,4/3);scale_xy(by['Cube35'],add=400)
 for a in actors:
  label=a.get_actor_label();p=a.get_actor_location();s=a.get_actor_scale3d()
  if label.startswith('UA_ArenaApron'):scale_xy(a,add=400)
  elif label.startswith('Ring_Post'):move(a,cx+(p.x-cx)*4/3,cy+(p.y-cy)*4/3)
  elif label.startswith('Ring_Rope'):
   move(a,cx+(p.x-cx)*4/3,cy+(p.y-cy)*4/3);a.set_actor_scale3d(unreal.Vector(s.x,s.y,s.z*4/3))
  elif label.startswith('Paint_'):
   if 'East' in label:move(a,p.x+200,p.y);a.set_actor_scale3d(unreal.Vector(s.x,s.y+4,s.z))
   elif 'West' in label:move(a,p.x-200,p.y);a.set_actor_scale3d(unreal.Vector(s.x,s.y+4,s.z))
   elif 'North' in label:move(a,p.x,p.y+200);a.set_actor_scale3d(unreal.Vector(s.x+4,s.y,s.z))
   elif 'South' in label:move(a,p.x,p.y-200);a.set_actor_scale3d(unreal.Vector(s.x+4,s.y,s.z))
  elif label.startswith('SM_Dummy_Stairs'):
   if p.x<cx-700:shift(a,-200,0)
   elif p.x>cx+700:shift(a,200,0)
   elif p.y>cy:shift(a,0,200)
   else:shift(a,0,-200)
  elif isinstance(a,(unreal.Character,unreal.PlayerStart)):move(a,cx+(p.x-cx)*4/3,cy+(p.y-cy)*4/3)
  elif label in ['Cube36','Cube37','Cube38','Cube39']:
   dx,dy={'Cube36':(-200,0),'Cube37':(200,0),'Cube38':(0,200),'Cube39':(0,-200)}[label];shift(a,dx,dy)
  elif label.startswith('UA_Barrier'):
   if p.x<-1000:shift(a,-200,0)
   elif p.x>1000:shift(a,200,0)
   elif p.y>cy:shift(a,0,200)
   else:shift(a,0,-200)
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)
for a in actors:
 old=before[a.get_name()]
 if a.get_name() not in changed:assert a.get_actor_location().distance(old['loc'])<.01 and a.get_actor_scale3d().distance(old['scale'])<.01 and a.get_actor_rotation()==old['rot'],a.get_actor_label()
 if a.get_actor_label().startswith('Ring_Post') or isinstance(a,(unreal.Character,unreal.PlayerStart)):assert a.get_actor_scale3d().distance(old['scale'])<.01
(OUT/'applied.json').write_text(json.dumps({'map':MAP,'backup':str(backup),'before_cm':1200,'after_cm':1600,'area_before_m2':144,'area_after_m2':256,'center':[cx,cy],'changed':changed,'actor_count':len(actors),'unrelated_preserved':True},indent=2));unreal.log('ROUND1_RING_EXPANDED_16M')
