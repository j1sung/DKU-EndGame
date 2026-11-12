"""Round4 놀이동산의 독립 메시와 팔레트를 만든다. 일반 Python에서 실행한다."""
import json
import math
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from generate_round3_dam import Mesh as BaseMesh

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/Round4Amusement/Sources'
PALETTE = dict(Cream=(.78,.69,.49), Coral=(.78,.18,.075), Teal=(.025,.34,.32),
               Yellow=(.96,.58,.08), Deck=(.44,.49,.46), Steel=(.12,.16,.17),
               Chrome=(.55,.62,.64), Glass=(.035,.12,.14), Paving=(.39,.36,.30),
               Leaf=(.12,.26,.065), White=(.91,.88,.73), Bulb=(1.,.68,.16))
manifest = dict(palette=PALETTE, meshes=[])


class Mesh(BaseMesh):
    def save(self, collision='none', nanite=True):
        self.faces.sort(key=lambda f:f[1])
        lines=['mtllib round4.mtl','o '+self.name]+['v %.5f %.5f %.5f'%(x,-y,z) for x,y,z in self.vertices]
        lines+=['vt 0 0','vt 1 0','vt 1 1'];last=None
        for ids,mat in self.faces:
            if mat!=last:lines.append('usemtl '+mat);last=mat
            lines.append('f '+' '.join('%d/%d'%(v+1,i+1) for i,v in enumerate(reversed(ids))))
        (OUT/(self.name+'.obj')).write_text('\n'.join(lines),encoding='utf-8')
        manifest['meshes'].append(dict(name=self.name,collision=collision,nanite=nanite))

    def sphere(self,p,r,mat,n=12):
        for j in range(6):
            a,b=-math.pi/2+j*math.pi/6,-math.pi/2+(j+1)*math.pi/6
            for i in range(n):
                c,d=i*math.tau/n,(i+1)*math.tau/n
                self.poly([(p[0]+r*math.cos(v)*math.cos(u),p[1]+r*math.cos(v)*math.sin(u),p[2]+r*math.sin(v)) for u,v in [(c,a),(d,a),(d,b),(c,b)]],mat)

    def taper(self,lo,hi,z0,z1,mat):
        a=[(-lo[0],-lo[1],z0),(lo[0],-lo[1],z0),(lo[0],lo[1],z0),(-lo[0],lo[1],z0)]
        b=[(-hi[0],-hi[1],z1),(hi[0],-hi[1],z1),(hi[0],hi[1],z1),(-hi[0],hi[1],z1)]
        self.poly(list(reversed(a)),mat);self.poly(b,mat)
        for i in range(4):self.poly([a[i],a[(i+1)%4],b[(i+1)%4],b[i]],mat)


def build():
    OUT.mkdir(parents=True,exist_ok=True)
    m=Mesh('SM_R4A_FightingDeck_1800x1200')
    m.box((0,0,-5),(1800,1200,40),'Steel')
    m.box((0,0,17.5),(1800,1200,15),'Deck')
    for y in [-594,594]:
        m.box((0,y,8),(1800,12,15),'Teal');m.box((0,y,-12),(1800,14,10),'Coral')
    for x in [-894,894]:
        m.box((x,0,8),(12,1200,15),'Teal');m.box((x,0,-12),(14,1200,10),'Coral')
    # 줄눈/표시는 Deck 재질에 합쳐 상면에 미세한 겹면을 만들지 않는다.
    m.save('box',False)
    m=Mesh('SM_R4A_PivotPedestal')
    m.taper((145,230),(72,150),0,800,'Cream')
    m.box((0,0,35),(330,520,70),'Steel')
    m.rod((0,-670,850),(0,670,850),70,'Steel',40)
    for y in [-677,677]:
        m.rod((0,y-10,850),(0,y+10,850),84,'Cream',40)
        m.rod((0,y-12,850),(0,y+12,850),58,'Teal',40)
    m.save('complex')
    m=Mesh('SM_R4A_PusherHousing')
    m.rod((0,0,0),(0,0,95),110,'Cream',40)
    m.rod((0,0,95),(0,0,110),95,'Steel',40)
    for i in range(8):
        t=i*math.tau/8;m.rod((95*math.cos(t),95*math.sin(t),0),(95*math.cos(t),95*math.sin(t),15),6,'Chrome',8)
    m.save('cylinder')
    m=Mesh('SM_R4A_PusherRod_100');m.rod((0,0,-50),(0,0,50),35,'Chrome',24);m.save(nanite=False)
    m=Mesh('SM_R4A_PusherHead');m.rod((0,0,-15),(0,0,7),78,'Steel',32);m.rod((0,0,7),(0,0,15),82,'Coral',32);m.save(nanite=False)
    m=Mesh('SM_R4A_BoardingBridge_200')
    m.box((0,0,0),(200,180,20),'Steel')
    for y in [-86,86]:m.box((0,y,11),(200,8,3),'Yellow')
    for x in range(-90,100,12):m.box((x,0,10.5),(3,165,1),'Chrome')
    m.save('box',False)
    m=Mesh('SM_R4A_SlidingGate')
    for y in [-88,88]:m.rod((0,y,-65),(0,y,65),6,'Cream',12)
    for z in [-55,55]:m.rod((0,-88,z),(0,88,z),5,'Coral',12)
    for y in range(-70,80,20):m.rod((0,y,-55),(0,y,55),3,'Teal',8)
    m.save('box',False)
    m=Mesh('SM_R4A_FencePanel_240')
    for x in [-115,115]:
        m.rod((x,0,0),(x,0,105),6,'Coral',12);m.sphere((x,0,108),8,'Cream')
    for z in [40,92]:m.rod((-115,0,z),(115,0,z),4,'Teal',12)
    m.save('box')
    m=Mesh('SM_R4A_StairFlight_875')
    for i in range(50):
        top=(i+1)*17.5;m.box((0,(i+.5)*25,top/2),(200,25,top),'Cream')
        m.box((0,(i+1)*25-3,top+.1),(198,5,.2),'Coral')
    for x in [-104,104]:
        for i in range(0,51,5):
            z=i*17.5;m.rod((x,i*25,z),(x,i*25,z+105),5,'Coral',12)
        for h in [45,100]:m.rod((x,0,h),(x,1250,875+h),4,'Teal',12)
    m.save('complex')
    for key in ['Cream','Steel','Teal','Coral','Paving']:
        m=Mesh('SM_R4A_Block_'+key);m.box((0,0,0),(100,100,100),key);m.save('box')
    m=Mesh('SM_R4A_OperatorBooth')
    m.box((0,0,95),(220,190,190),'Cream')
    m.box((0,-96,135),(190,2,85),'Glass')
    for x in [-105,105]:m.box((x,0,135),(2,170,85),'Glass')
    m.box((0,-112,85),(180,50,18),'Steel')
    for i,key in enumerate(['Coral','Yellow','Teal']):m.rod((-40+i*40,-124,95),(-40+i*40,-124,104),11,key,16)
    m.rod((65,-115,94),(65,-115,130),4,'Chrome',12);m.sphere((65,-115,132),9,'Coral')
    for i in range(8):
        m.box((-105+(i+.5)*210/8,0,201),(210/8,220,20),'Coral' if i%2 else 'Cream')
        m.box((-105+(i+.5)*210/8,-115,183),(210/8,12,36),'Coral' if i%2 else 'Cream')
    m.save('box')
    m=Mesh('SM_R4A_ConcessionBooth')
    m.box((0,0,160),(340,300,320),'Cream');m.box((0,-152,190),(285,6,145),'Teal')
    m.box((0,-185,115),(330,65,25),'Coral')
    for i in range(10):
        x=-180+(i+.5)*36
        m.poly([(x-18,-225,240),(x+18,-225,240),(x+18,-150,310),(x-18,-150,310)],'Coral' if i%2 else 'White')
        m.box((x,-225,230),(36,8,20),'Coral' if i%2 else 'White')
    m.box((0,0,332),(370,330,24),'Coral')
    for x in range(-160,180,40):m.sphere((x,-171,330),9,'Bulb')
    m.save('box')
    m=Mesh('SM_R4A_Planter')
    m.box((0,0,30),(300,150,60),'Cream');m.box((0,0,62),(280,130,6),'Steel')
    for i in range(7):m.sphere((-115+i*38,20*math.sin(i),95+(i%3)*15),45,'Leaf')
    for i in range(5):m.sphere((-95+i*45,-40,88),12,'Yellow' if i%2 else 'Coral')
    m.save('box')
    m=Mesh('SM_R4A_FerrisFrame_4000')
    # 관람차는 XZ 평면, 중심은 Z=2250. 발판/대기줄과는 분리된 배경이다.
    for y in [-290,290]:
        for i in range(80):
            a,b=i*math.tau/80,(i+1)*math.tau/80
            m.rod((2000*math.cos(a),y,2250+2000*math.sin(a)),(2000*math.cos(b),y,2250+2000*math.sin(b)),15,'Coral',8)
        for i in range(16):
            a=i*math.tau/16;m.rod((0,y,2250),(2000*math.cos(a),y,2250+2000*math.sin(a)),10,'Cream',8)
        for x in [-900,900]:
            m.rod((x,y*2,0),(0,y,2250),60,'Cream',12)
            m.box((x,y*2,30),(300,300,60),'Cream')
    m.rod((0,-340,2250),(0,340,2250),150,'Teal',40)
    m.save()
    m=Mesh('SM_R4A_FerrisCabin')
    m.box((0,0,55),(240,240,110),'Coral');m.box((0,0,145),(220,220,70),'Glass')
    m.box((0,0,190),(250,250,20),'Cream');m.rod((0,0,200),(0,0,300),9,'Steel',12)
    m.save()
    m=Mesh('SM_R4A_CoasterBackdrop')
    def point(t,y):return (t*5200,y,1200+700*math.sin(t*math.tau*1.4)+300*math.sin(t*math.pi))
    for j in range(100):
        t,u=j/100,(j+1)/100
        for y in [-75,75]:m.rod(point(t,y),point(u,y),15,'Coral',8)
        m.rod(point(t,-75),point(t,75),6,'Cream',6)
        if j%5==0:
            p=point(t,0);m.rod((p[0],0,0),p,25,'Teal',12)
    m.save()
    m=Mesh('SM_R4A_DropTower')
    m.rod((0,0,0),(0,0,2800),100,'Cream',24)
    for z in [1600,2700]:m.rod((0,0,z),(0,0,z+90),230,'Coral',32)
    m.rod((0,0,2790),(0,0,3100),20,'Teal',16);m.save()
    m=Mesh('SM_R4A_ArrowMark')
    m.poly([(-35,-15,0),(0,-15,0),(0,-35,0),(50,0,0),(0,35,0),(0,15,0),(-35,15,0)],'Coral');m.save(nanite=False)
    m=Mesh('SM_R4A_SignBoard');m.box((0,0,0),(180,12,65),'Steel')
    for x in [-88,88]:m.box((x,0,0),(4,15,65),'Cream')
    m.save()
    (OUT/'round4.mtl').write_text('\n'.join('newmtl %s\nKd %s'%(k,' '.join(map(str,v))) for k,v in PALETTE.items()),encoding='utf-8')
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('Round4 meshes:',len(manifest['meshes']))


if __name__=='__main__':build()

