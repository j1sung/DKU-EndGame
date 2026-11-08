"""도심 스파링장 전용 메시·간판 원본 생성. 일반 Python에서 실행 후 apply_urban_arena.py 사용."""
import math,json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/UrbanArenaWork/Source'; OUT.mkdir(parents=True,exist_ok=True)
PALETTE={'Plaster':(.47,.43,.35),'Ivory':(.66,.63,.53),'Brick':(.32,.12,.075),'Concrete':(.29,.29,.265),'Mortar':(.19,.18,.155),'Metal':(.055,.075,.072),'Glass':(.045,.105,.115),'GlassLight':(.14,.22,.23),'Wood':(.30,.17,.075),'Rust':(.35,.13,.055),'Red':(.48,.055,.035),'Teal':(.025,.21,.22),'Canvas':(.69,.53,.32),'Cream':(.82,.75,.58),'Asphalt':(.085,.09,.085),'Mat':(.53,.48,.36),'White':(.86,.83,.70),'Blue':(.045,.14,.32),'Light':(.92,.66,.30)}
manifest={'palette':PALETTE,'meshes':[],'signs':[]}
class Mesh:
 def __init__(self,name): self.name=name;self.v=[];self.f=[]
 def poly(self,pts,mat,uv=None):
  n=len(self.v); self.v+=pts
  uv=uv or [(0,0),(1,0),(1,1),(0,1)][:len(pts)]
  for i in range(1,len(pts)-1):self.f.append(([n,n+i,n+i+1],mat,[uv[0],uv[i],uv[i+1]]))
 def box(self,c,s,mat,yaw=0):
  x,y,z=c;w,d,h=[v/2 for v in s]; a=math.radians(yaw);co,si=math.cos(a),math.sin(a)
  p=[(x+u*co-v*si,y+u*si+v*co,z+t) for u,v,t in [(-w,-d,-h),(w,-d,-h),(w,d,-h),(-w,d,-h),(-w,-d,h),(w,-d,h),(w,d,h),(-w,d,h)]]
  for f in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:self.poly([p[i] for i in f],mat)
 def rod(self,a,b,r,mat,n=10):
  d=[b[i]-a[i] for i in range(3)];L=math.sqrt(sum(x*x for x in d));d=[x/L for x in d];q=[0,0,1] if abs(d[2])<.9 else [1,0,0]
  u=[d[1]*q[2]-d[2]*q[1],d[2]*q[0]-d[0]*q[2],d[0]*q[1]-d[1]*q[0]];le=math.sqrt(sum(x*x for x in u));u=[x/le for x in u];v=[d[1]*u[2]-d[2]*u[1],d[2]*u[0]-d[0]*u[2],d[0]*u[1]-d[1]*u[0]]
  rings=[[(p[j]+r*(u[j]*math.cos(i*2*math.pi/n)+v[j]*math.sin(i*2*math.pi/n))) for j in range(3)] for p in [a,b] for i in range(n)]
  self.poly(list(reversed(rings[:n])),mat,[(0,0)]*n);self.poly(rings[n:],mat,[(0,0)]*n)
  for i in range(n):j=(i+1)%n;self.poly([rings[i],rings[j],rings[n+j],rings[n+i]],mat)
 def save(self,**kwargs):
  # Interchange OBJ uses Z up and flips Y. Supply explicit per-corner UVs.
  self.f.sort(key=lambda face:face[1])
  lines=['mtllib urban.mtl','o '+self.name]+['v %.5f %.5f %.5f'%(x,-y,z) for x,y,z in self.v]
  for ids,mat,uv in self.f:
   for u,v in reversed(uv):lines.append('vt %.5f %.5f'%(u,v))
  off=1;last=None
  for ids,mat,uv in self.f:
   if mat!=last:lines.append('usemtl '+mat);last=mat
   lines.append('f '+' '.join('%d/%d'%(i+1,off+j) for j,i in enumerate(reversed(ids))));off+=3
  (OUT/(self.name+'.obj')).write_text('\n'.join(lines));manifest['meshes'].append(dict(name=self.name,**kwargs))
# Korean shop signs are authored textures, with real readable lettering.
font='C:/Windows/Fonts/malgunbd.ttf'
signs=[('청춘 체육관','STREET SPARRING CLUB','Teal'),('중앙상회','SINCE 1988  /  DAILY GOODS','Red'),('골목식당','국수 · 만두 · 따뜻한 한 끼','Brick'),('도시 커피','URBAN COFFEE  /  TAKE OUT','Metal'),('동네약국','건강한 하루의 시작','Teal'),('대성 전파사','수리 · 조명 · 전기','Blue')]
for i,(title,sub,bg) in enumerate(signs):
 im=Image.new('RGB',(1024,256),tuple(int(v**(1/2.2)*255) for v in PALETTE[bg]));dr=ImageDraw.Draw(im);dr.rectangle((10,10,1013,245),outline=(224,207,166),width=3)
 dr.text((512,88),title,font=ImageFont.truetype(font,88),fill=(245,230,193),anchor='mm');dr.text((512,195),sub,font=ImageFont.truetype(font,28),fill=(226,212,178),anchor='mm');im.save(OUT/('T_Urban_Sign_%02d.png'%i));manifest['signs'].append('Sign%02d'%i)
# Perimeter buildings retain the measured graybox footprints and heights.
buildings=[('Cube25',2000,1000,2500),('Cube26',1000,1000,2500),('Cube27',1000,1000,2500),('Cube28',1000,1000,2500),('Cube29',2250,750,1250),('Cube30',1000,2000,2500),('Cube31',1000,1000,2500),('Cube32',500,1000,1875),('Cube33',500,1000,1875),('Cube34',500,1000,1875)]
for k,(label,w,d,h) in enumerate(buildings):
 m=Mesh('SM_Urban_Building_%02d'%k);body=['Ivory','Plaster','Brick','Concrete'][k%4];m.box((0,0,h/2),(w,d,h),body)
 def face(u,dep,z,sz,mat,side):
  xx,yy,zz=sz
  if side==0:m.box((u,-d/2-dep,z),sz,mat)
  elif side==1:m.box((w/2+dep,u,z),(yy,xx,zz),mat)
  elif side==2:m.box((-u,d/2+dep,z),sz,mat)
  else:m.box((-w/2-dep,-u,z),(yy,xx,zz),mat)
 for side in range(4):
  width=w if side%2==0 else d
  cols=max(1,int((width-100)//260));spacing=(width-100)/cols
  for z in range(390,int(h)-100,340):
   face(0,5,z-150,(width,14,14),'Mortar',side)
   for col in range(cols):
    u=-width/2+50+(col+.5)*spacing;ww=min(180,spacing-42)
    face(u,13,z,(ww+18,20,190),'Metal',side);face(u,25,z,(ww,8,166),'GlassLight' if (col+z//340+k)%5==0 else 'Glass',side)
    face(u,33,z,(6,10,166),'Metal',side);face(u,38,z-95,(ww+35,60,14),'Concrete',side)
    if (col+k+z//340)%4==0:
     face(u+ww*.30,62,z-70,(62,65,48),'Ivory',side)
     for stripe in range(5):face(u+ww*.30,97,z-86+stripe*7,(48,4,2),'Metal',side)
  # Ground-floor retail bays with striped pitched awnings, glazing and signs.
  bays=max(1,int(width//500)); bw=(width-70)/bays
  for bay in range(bays):
   u=-width/2+35+(bay+.5)*bw
   face(u,16,140,(bw-24,28,250),'Metal',side);face(u,34,144,(bw-54,10,212),'Glass',side);face(u,45,144,(10,12,225),'Ivory',side)
   face(u,55,306,(bw-16,44,95),'Metal',side)
   # Sign quad outward normal, UV left to right viewed from street.
   pts=[(u+bw*.46,-d/2-80,263),(u-bw*.46,-d/2-80,263),(u-bw*.46,-d/2-80,349),(u+bw*.46,-d/2-80,349)]
   if side:
    pts=[((w/2+(-y-d/2)) if side==1 else -x if side==2 else -w/2-(-y-d/2),x if side==1 else d/2+(-y-d/2) if side==2 else -x,z) for x,y,z in pts]
   m.poly(pts,'Sign%02d'%((k+bay)%6))
   strips=max(3,int(bw//60))
   for t in range(strips):
    uu=u-bw/2+(t+.5)*bw/strips
    face(uu,110,249,(bw/strips,180,12),'Canvas' if t%2 else ['Red','Teal','Cream'][k%3],side)
    face(uu,202,235,(bw/strips,10,28),'Canvas' if t%2 else ['Red','Teal','Cream'][k%3],side)
  # Downpipe, cornices and facade panel seams.
  face(width/2-28,24,h/2,(12,20,h),'Rust',side)
  face(0,16,h-15,(width+32,44,36),'Ivory',side)
  for u in range(int(-width/2+100),int(width/2),250):face(u,1,h/2,(3,4,h-60),'Mortar',side)
 # Rooftop parapets, access hut, HVAC and ductwork.
 for y in [-d/2+12,d/2-12]:m.box((0,y,h+45),(w,24,90),'Concrete')
 for x in [-w/2+12,w/2-12]:m.box((x,0,h+45),(24,d,90),'Concrete')
 m.box((-w*.2,d*.18,h+120),(min(w*.4,360),min(d*.4,300),240),'Plaster');m.box((-w*.2,d*.18,h+249),(min(w*.4,360)+25,min(d*.4,300)+25,18),'Metal')
 for j in range(max(2,int(w//420))):
  x=-w*.33+j*290;y=-d*.22;m.box((x,y,h+65),(170,135,120),'Ivory');m.rod((x,y,h+126),(x,y,h+130),48,'Metal',20)
  for a in range(6):m.box((x,y-69,h+25+a*14),(145,5,5),'Metal')
  m.box((x,y+130,h+42),(65,165,72),'Concrete')
 m.save(replace=label,collision=True)
# Rounded corner commercial building.
m=Mesh('SM_Urban_CornerTower');m.rod((0,0,0),(0,0,2000),1000,'Plaster',64)
for z in range(200,2001,340):
 for i in range(32):
  a=i*2*math.pi/32;x,y=1007*math.cos(a),1007*math.sin(a);m.box((x,y,z),(145,20,170),'Glass',math.degrees(a)+90)
 for i in range(64):
  a=i*2*math.pi/64;m.box((1005*math.cos(a),1005*math.sin(a),z-110),(100,24,16),'Ivory',math.degrees(a)+90)
for i in range(4):m.box((-300+i*190,100,2070),(140,120,140),'Ivory')
m.save(replace='Cylinder',collision=True)
# Reusable steel grandstand: open bracing, timber planks, backs, aisle and rear rails.
m=Mesh('SM_Urban_Grandstand');length=1100
for row in range(5):
 y=row*85;z=65+row*44
 m.box((0,y,z-18),(length,84,12),'Concrete')
 for x in [-360,360]:
  m.box((x,y,z+14),(380,36,12),'Wood');m.box((x,y+25,z+43),(380,8,40),'Wood')
 for x in [-530,-220,220,530]:
  m.box((x,y,z/2-12),(12,12,z-24),'Metal')
  if row<4:m.rod((x,y,5),(x,y+85,z+26),5,'Metal')
 m.box((0,y,z-12),(118,80,16),'Metal');m.box((0,y-40,z-1),(118,7,4),'Cream')
for x in [-550,550]:
 for y in [0,170,340]:m.rod((x,y,0),(x,y,350),4,'Metal')
 m.rod((x,0,130),(x,340,350),4,'Metal')
for z in [280,350]:m.rod((-550,350,z),(550,350,z),4,'Metal')
for x in range(-550,551,110):m.rod((x,350,200),(x,350,350),3,'Metal')
m.save(collision=False)
# Pavement, arena skirt, lamp, bins and barriers are discrete reusable assets.
m=Mesh('SM_Urban_PavingTile');m.box((0,0,-3),(99,99,6),'Concrete');m.save(collision=False)
m=Mesh('SM_Urban_ArenaApron');
for y in [-638,638]:m.box((0,y,110),(1280,12,180),'Teal');m.box((0,y,194),(1280,22,12),'Cream')
for x in [-638,638]:m.box((x,0,110),(12,1280,180),'Teal');m.box((x,0,194),(22,1280,12),'Cream')
m.save(collision=False)
m=Mesh('SM_Urban_StreetLamp');m.box((0,0,8),(65,65,16),'Metal');m.rod((0,0,15),(0,0,560),7,'Metal');m.rod((0,0,545),(90,0,545),5,'Metal');m.box((100,0,535),(100,45,20),'Metal');m.box((100,0,523),(86,33,3),'Light');m.save(collision=False)
m=Mesh('SM_Urban_Barrier');
for x in [-100,100]:m.box((x,0,8),(16,65,16),'Metal');m.rod((x,0,8),(x,0,110),4,'Metal')
for z in [40,105]:m.rod((-100,0,z),(100,0,z),4,'Metal')
for x in range(-80,81,20):m.rod((x,0,40),(x,0,105),2,'Metal')
m.save(collision=False)
m=Mesh('SM_Urban_Bin');m.rod((0,0,5),(0,0,85),29,'Teal',16);m.rod((0,0,85),(0,0,91),32,'Metal',16);m.box((0,-29,65),(32,4,18),'Metal');m.save(collision=False)
m=Mesh('SM_Urban_PlazaPaving')
for x in range(-2400,2400,200):
 for y in range(-2400,2400,200):m.box((x+100,y+100,-3),(198,198,6),'Concrete' if (x//200+y//200)%5 else 'Ivory')
# Stone curb separates the central pedestrian plaza from the perimeter service lane.
for y in [-2420,2420]:m.box((0,y,6),(4880,35,18),'Ivory')
for x in [-2420,2420]:m.box((x,0,6),(35,4880,18),'Ivory')
m.save(collision=False)
(OUT/'urban.mtl').write_text('\n'.join('newmtl '+k+'\nKd .5 .5 .5' for k in list(PALETTE)+manifest['signs']))
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('Generated',len(manifest['meshes']),'meshes and',len(signs),'sign textures at',OUT)
