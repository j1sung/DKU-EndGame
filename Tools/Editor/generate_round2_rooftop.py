"""현재 Organic 배치에 맞는 옥상 설비와 외장 OBJ 원본 생성(일반 Python)."""
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'Saved/RooftopWork/Sources'
OUT.mkdir(parents=True, exist_ok=True)
PALETTE = {'Plaster':(.46,.45,.40), 'Coping':(.60,.58,.51),
           'Roof':(.15,.14,.125), 'Seam':(.085,.10,.10),
           'Steel':(.105,.14,.145), 'Zinc':(.40,.46,.46),
           'Ivory':(.65,.65,.57), 'Dark':(.022,.032,.035),
           'Orange':(.65,.23,.075), 'Teal':(.055,.25,.27),
           'Glass':(.07,.14,.16), 'Stain':(.28,.29,.25)}
manifest = {'palette':PALETTE, 'meshes':[], 'placements':[]}


class Mesh:
    def __init__(self, name):
        self.name, self.v, self.f = name, [], []

    def poly(self, pts, mat):
        offset = len(self.v)
        self.v.extend(pts)
        for i in range(1,len(pts)-1):
            self.f.append(([offset,offset+i,offset+i+1],mat))

    def box(self, c, size, mat, yaw=0):
        x,y,z=c; w,d,h=[s/2 for s in size]
        co,si=math.cos(math.radians(yaw)),math.sin(math.radians(yaw))
        pts=[(x+u*co-v*si,y+u*si+v*co,z+t) for u,v,t in
             [(-w,-d,-h),(w,-d,-h),(w,d,-h),(-w,d,-h),(-w,-d,h),(w,-d,h),(w,d,h),(-w,d,h)]]
        for face in [(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:
            self.poly([pts[i] for i in face],mat)

    def rod(self,a,b,r,mat,n=12):
        delta=[b[i]-a[i] for i in range(3)]; length=math.sqrt(sum(v*v for v in delta))
        if length<.001:return
        delta=[v/length for v in delta];q=(0,0,1) if abs(delta[2])<.9 else (1,0,0)
        u=[delta[1]*q[2]-delta[2]*q[1],delta[2]*q[0]-delta[0]*q[2],delta[0]*q[1]-delta[1]*q[0]]
        length=math.sqrt(sum(v*v for v in u));u=[v/length for v in u]
        v=[delta[1]*u[2]-delta[2]*u[1],delta[2]*u[0]-delta[0]*u[2],delta[0]*u[1]-delta[1]*u[0]]
        rings=[[(p[j]+r*(u[j]*math.cos(i*math.tau/n)+v[j]*math.sin(i*math.tau/n))) for j in range(3)] for p in (a,b) for i in range(n)]
        self.poly(list(reversed(rings[:n])),mat);self.poly(rings[n:],mat)
        for i in range(n):
            k=(i+1)%n;self.poly([rings[i],rings[k],rings[n+k],rings[n+i]],mat)

    def ring(self,c,r,tube,mat,n=32):
        x,y,z=c
        for i in range(n):
            a,b=i*math.tau/n,(i+1)*math.tau/n
            self.rod((x+r*math.cos(a),y+r*math.sin(a),z),(x+r*math.cos(b),y+r*math.sin(b),z),tube,mat,6)

    def save(self,collision=False,**extra):
        self.f.sort(key=lambda f:f[1])
        lines=['mtllib rooftop.mtl','o '+self.name]+['v %.5f %.5f %.5f'%(x,-y,z) for x,y,z in self.v]
        lines+=['vt 0 0','vt 1 0','vt 1 1']
        last=None
        for ids,mat in self.f:
            if mat!=last:lines.append('usemtl '+mat);last=mat
            lines.append('f '+' '.join('%d/%d'%(idx+1,j+1) for j,idx in enumerate(reversed(ids))))
        (OUT/(self.name+'.obj')).write_text('\n'.join(lines),encoding='utf-8')
        manifest['meshes'].append(dict(name=self.name,collision=collision,**extra))


def fan(m,x,y,z,r):
    m.rod((x,y,z-5),(x,y,z),r,'Dark',32)
    for radius in [r*.35,r*.65,r*.95]:m.ring((x,y,z+3),radius,1.1,'Zinc')
    for angle in range(0,360,45):
        a=math.radians(angle);m.rod((x,y,z+4),(x+math.cos(a)*r,y+math.sin(a)*r,z+4),1.1,'Zinc',5)
    for angle in range(0,360,72):m.box((x,y,z+1),(r*1.4,r*.22,3),'Steel',angle)
    m.rod((x,y,z),(x,y,z+9),r*.13,'Ivory',16)


for kind,w,d,h,count in [('HVAC_Double',340,180,145,2),('HVAC_Single',175,175,125,1)]:
    m=Mesh('SM_RT_'+kind)
    for x in [-w*.38,w*.38]:m.box((x,0,12),(22,d+15,24),'Steel')
    m.box((0,0,h/2+20),(w,d,h),'Ivory');m.box((0,0,h+23),(w+8,d+8,8),'Zinc')
    for z in range(45,h+5,12):
        for y in [-d/2-1,d/2+1]:m.box((0,y,z),(w-24,4,4),'Steel')
    for x in [-w/2,w/2]:
        m.box((x,0,h/2+20),(5,d-18,h-18),'Zinc')
        for y in range(-int(d/2)+20,int(d/2)-10,16):m.box((x*1.008,y,h/2+20),(6,3,h-35),'Steel')
    for i in range(count):fan(m,(i-(count-1)/2)*175,0,h+30,67)
    m.box((w/2-32,-d/2-4,70),(42,5,65),'Teal')
    for x in [-w/2+15,w/2-15]:
        for y in [-d/2+15,d/2-15]:m.rod((x,y,h+25),(x,y,h+28),3,'Dark',8)
    m.save(True)

m=Mesh('SM_RT_ExhaustFan');m.box((0,0,20),(160,160,40),'Plaster')
m.rod((0,0,40),(0,0,110),59,'Zinc',32);m.ring((0,0,108),66,8,'Zinc')
fan(m,0,0,112,57)
m.save(True)

m=Mesh('SM_RT_DuctStraight');m.box((0,0,85),(100,400,100),'Zinc')
for y in [-190,-100,0,100,190]:
    m.box((0,y,138),(110,5,6),'Ivory')
    for x in [-53,53]:m.box((x,y,85),(5,6,100),'Ivory')
for y in [-145,145]:m.box((0,y,20),(140,35,40),'Steel')
m.save(True)

m=Mesh('SM_RT_DuctElbow')
# 환경 설비용 사각 덕트의 분절 엘보: 뚜껑·접합 띠까지 실제 기하로 구성한다.
for i in range(8):
    a=i*math.pi/16;b=(i+1)*math.pi/16
    for side in [-50,50]:
        m.poly([(side,120*math.sin(a),75+120*math.cos(a)),(side,220*math.sin(a),75+220*math.cos(a)),(side,220*math.sin(b),75+220*math.cos(b)),(side,120*math.sin(b),75+120*math.cos(b))],'Zinc')
    for r in [120,220]:m.poly([(-50,r*math.sin(a),75+r*math.cos(a)),(50,r*math.sin(a),75+r*math.cos(a)),(50,r*math.sin(b),75+r*math.cos(b)),(-50,r*math.sin(b),75+r*math.cos(b))],'Zinc')
    m.rod((-53,220*math.sin(a),75+220*math.cos(a)),(53,220*math.sin(a),75+220*math.cos(a)),3,'Ivory',6)
m.box((0,218,75),(108,8,108),'Steel');m.save(True)

m=Mesh('SM_RT_WaterTank')
for x in [-65,65]:
    for y in [-65,65]:m.box((x,y,25),(18,18,50),'Steel')
m.rod((0,0,50),(0,0,290),105,'Zinc',40)
for z in [58,120,220,283]:m.ring((0,0,z),106,4,'Ivory')
m.rod((0,0,290),(0,0,312),28,'Steel',20)
for z in range(25,301,32):m.rod((107,-22,z),(107,22,z),2.5,'Steel',6)
for y in [-25,25]:m.rod((107,y,20),(107,y,312),3,'Steel')
m.rod((0,105,115),(0,150,115),11,'Zinc');m.rod((0,150,115),(0,150,18),11,'Zinc')
m.save(True)

m=Mesh('SM_RT_AccessHut');m.box((0,0,145),(310,270,290),'Plaster')
m.box((0,0,295),(335,295,16),'Coping');m.box((0,-138,123),(110,10,235),'Steel')
m.box((0,-145,125),(90,6,205),'Teal')
for z in range(175,221,8):m.box((0,-150,z),(70,4,3),'Dark')
m.rod((30,-154,95),(30,-154,120),3,'Zinc');m.box((0,-165,17),(150,60,34),'Coping')
m.save(True)

m=Mesh('SM_RT_UtilityPipes')
for x in [-60,0,60]:
    m.rod((x,0,15),(x,0,185),9,'Zinc');m.rod((x,0,185),(x,160,185),9,'Zinc')
    for z in [35,155]:m.ring((x,0,z),11,3,'Steel',16)
for z in [40,150]:m.box((0,4,z),(170,20,10),'Steel')
m.box((90,45,70),(70,65,120),'Teal');m.save(False)

m=Mesh('SM_RT_CableTray')
for x in [-28,28]:m.box((x,0,12),(5,400,24),'Steel')
for y in range(-195,200,25):m.box((0,y,6),(58,5,5),'Zinc')
for x in [-15,-3,10]:m.rod((x,-200,17),(x,200,17),3,'Dark',8)
m.save(False)

m=Mesh('SM_RT_WallAC');m.box((0,0,50),(140,60,90),'Ivory')
for x in [-50,50]:m.box((x,15,0),(12,105,10),'Steel')
for z in range(20,85,8):m.box((0,-32,z),(120,4,3),'Steel')
m.box((45,-35,53),(23,5,60),'Teal');m.save(False)

m=Mesh('SM_RT_Drain');m.box((0,0,1),(65,65,2),'Steel')
for x in range(-25,26,10):m.box((x,0,3),(4,55,2),'Zinc')
m.save(False)

for variant in ['Parapet','Hybrid','Rail']:
    m=Mesh('SM_RT_'+variant+'_240')
    if variant!='Rail':
        h=125 if variant=='Parapet' else 55
        m.box((0,0,h/2),(240,26,h),'Plaster');m.box((0,0,h+3),(242,34,10),'Coping')
        m.box((-119,-13.4,h/2),(2,1,h-10),'Stain')
    if variant!='Parapet':
        base=60 if variant=='Hybrid' else 0
        for x in [-117,117]:m.rod((x,0,base),(x,0,135),4,'Steel')
        for z in [base+15,100,135]:m.rod((-120,0,z),(120,0,z),2.7,'Orange' if z==135 else 'Steel')
        for x in [-80,-40,0,40,80]:m.rod((x,0,base+15),(x,0,135),1,'Steel',5)
    m.save(True)

before=json.loads((ROOT/'Saved/RooftopWork/before.json').read_text())
actors={a['label']:a for a in before['actors']}
# 경사 패널은 기존 상승량을 보존하고 난간 높이만 줄인다.
rises={}
for a in actors.values():
    if not a['label'].startswith('OR_FencePanel_Stairs'):continue
    mesh=a['meshes'][0].split('.')[-1]
    if '_Rise_' in mesh:
        value=mesh.split('_Rise_')[1];rise=float(value[1:].replace('_','.'))*(-1 if value[0]=='N' else 1)
    else:rise=0
    rises[mesh]=rise
for old,rise in rises.items():
    m=Mesh('SM_RT_StairRail_'+old.replace('SM_Round2_FencePanel_',''))
    def p(x,z):return (x,0,z+rise*x/240)
    for x in [-120,120]:m.rod(p(x,0),p(x,135),4,'Steel')
    for z in [25,85,135]:m.rod(p(-120,z),p(120,z),3,'Orange' if z==135 else 'Steel')
    for x in [-60,0,60]:m.rod(p(x,25),p(x,135),1.7,'Steel')
    m.save(True,old_stair=old)

for i,label in enumerate(['SM_Dummy_Block_100x10'+str(n) for n in range(6)]):
    a=actors[label];o,e=a['bounds'];w,d,h=[2*v for v in e];top=o[2]+e[2]
    # 원본 박스는 보행 충돌을 계속 담당하고 이 메시들은 외장/방수층만 담당한다.
    m=Mesh('SM_RT_Roof_%02d'%i)
    nx,ny=math.ceil(w/250),math.ceil(d/250)
    for x in range(nx):
        for y in range(ny):m.box((-w/2+(x+.5)*w/nx,-d/2+(y+.5)*d/ny,0),(w/nx-1.5,d/ny-1.5,1.5),'Roof')
    m.save(False)
    manifest['placements'].append(dict(mesh=m.name,loc=[o[0],o[1],top+3.8],folder='Roofs'))
    m=Mesh('SM_RT_Facade_%02d'%i)
    def face(u,depth,z,size,mat,side):
        ww,dd,hh=size
        if side==0:m.box((u,-d/2-depth,z),(ww,dd,hh),mat)
        elif side==1:m.box((w/2+depth,u,z),(dd,ww,hh),mat)
        elif side==2:m.box((u,d/2+depth,z),(ww,dd,hh),mat)
        else:m.box((-w/2-depth,u,z),(dd,ww,hh),mat)
    for side in range(4):
        width=w if side%2==0 else d
        face(0,5,h-70,(width,14,70),'Orange' if i in [0,3] else 'Teal',side)
        face(0,13,h-18,(width+24,32,25),'Coping',side)
        for z in range(180,int(h)-190,340):
            face(0,3,z-100,(width,8,8),'Stain',side)
            count=max(1,int(width/300));spacing=width/count
            for j in range(count):
                u=-width/2+(j+.5)*spacing
                face(u,7,z,(155,16,165),'Steel',side);face(u,17,z,(136,5,145),'Glass',side)
                face(u,23,z,(5,7,145),'Zinc',side);face(u,25,z-85,(175,38,12),'Coping',side)
        for u in [-width/2+55,width/2-55]:
            face(u,22,h/2,(12,18,h-40),'Zinc',side)
            for z in range(150,int(h),280):face(u,29,z,(23,8,10),'Steel',side)
    m.save(False)
    manifest['placements'].append(dict(mesh=m.name,loc=[o[0],o[1],o[2]-e[2]],folder='Facades'))

# 주변 도시는 실제 플레이 바닥에서 18m 아래 도로를 두고 독립된 건물로 둘러싼다.
for variant,w,d,h in [(0,1350,1500,2850),(1,1800,1300,3350),(2,1200,1700,2400)]:
    m=Mesh('SM_RT_BackgroundBuilding_%02d'%variant)
    m.box((0,0,h/2),(w,d,h),'Plaster' if variant!=1 else 'Stain')
    for x in [-w/2,w/2]:m.box((x,0,h+55),(24,d,110),'Coping')
    for y in [-d/2,d/2]:m.box((0,y,h+55),(w,24,110),'Coping')
    for z in range(250,h-80,330):
        for x in range(-w//2+150,w//2-50,270):
            for y in [-d/2-3,d/2+3]:
                m.box((x,y,z),(145,12,165),'Steel');m.box((x,y*1.004,z),(125,8,140),'Glass')
        for y in range(-d//2+150,d//2-50,270):
            for x in [-w/2-3,w/2+3]:m.box((x,y,z),(12,145,165),'Glass')
    for j in [-1,1]:
        m.box((j*260,0,h+70),(220,160,140),'Ivory');fan(m,j*260,0,h+144,58)
    m.box((w*.2,d*.25,h+135),(280,260,270),'Plaster')
    m.save(False)

(OUT/'rooftop.mtl').write_text('\n'.join('newmtl '+k+'\nKd '+' '.join(map(str,v)) for k,v in PALETTE.items()))
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('Rooftop meshes:',len(manifest['meshes']))
