"""Round3 댐 정비장용 모듈 OBJ를 생성한다. 일반 Python에서 실행한다."""
import json
import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/Round3Theme/Sources'
PALETTE = dict(Concrete=(.57,.55,.46), Coping=(.68,.65,.55), Wood=(.32,.17,.075),
               WoodLight=(.48,.29,.13), Steel=(.055,.075,.073), Teal=(.025,.19,.17),
               Orange=(.8,.24,.025), Yellow=(.8,.52,.035), Rock=(.36,.37,.32),
               Moss=(.11,.17,.065), Water=(.025,.24,.22))
manifest = dict(palette=PALETTE, meshes=[])


class Mesh:
    def __init__(self, name):
        self.name, self.vertices, self.faces = name, [], []

    def poly(self, pts, mat):
        offset = len(self.vertices)
        self.vertices.extend(pts)
        for i in range(1, len(pts)-1):
            self.faces.append(([offset, offset+i, offset+i+1], mat))

    def box(self, center, size, mat):
        x,y,z = center; w,d,h = [v/2 for v in size]
        pts=[(x+u,y+v,z+t) for u,v,t in
             [(-w,-d,-h),(w,-d,-h),(w,d,-h),(-w,d,-h),(-w,-d,h),(w,-d,h),(w,d,h),(-w,d,h)]]
        for face in [(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:
            self.poly([pts[i] for i in face],mat)

    def rod(self,a,b,r,mat,n=16):
        delta=[b[i]-a[i] for i in range(3)]; length=math.sqrt(sum(v*v for v in delta))
        if length<.001:return
        delta=[v/length for v in delta]; q=(0,0,1) if abs(delta[2])<.9 else (1,0,0)
        u=[delta[1]*q[2]-delta[2]*q[1],delta[2]*q[0]-delta[0]*q[2],delta[0]*q[1]-delta[1]*q[0]]
        length=math.sqrt(sum(v*v for v in u)); u=[v/length for v in u]
        v=[delta[1]*u[2]-delta[2]*u[1],delta[2]*u[0]-delta[0]*u[2],delta[0]*u[1]-delta[1]*u[0]]
        rings=[tuple(p[j]+r*(u[j]*math.cos(i*math.tau/n)+v[j]*math.sin(i*math.tau/n)) for j in range(3)) for p in (a,b) for i in range(n)]
        self.poly(list(reversed(rings[:n])),mat); self.poly(rings[n:],mat)
        for i in range(n):
            k=(i+1)%n; self.poly([rings[i],rings[k],rings[n+k],rings[n+i]],mat)

    def ring(self, center, radius, tube, mat, n=32):
        x,y,z=center
        for i in range(n):
            a,b=i*math.tau/n,(i+1)*math.tau/n
            self.rod((x+radius*math.cos(a),y+radius*math.sin(a),z),(x+radius*math.cos(b),y+radius*math.sin(b),z),tube,mat,6)

    def save(self, collision='none', nanite=True):
        self.faces.sort(key=lambda f:f[1])
        lines=['mtllib round3.mtl','o '+self.name]+['v %.5f %.5f %.5f'%(x,-y,z) for x,y,z in self.vertices]
        lines += ['vt 0 0','vt 1 0','vt 1 1']; last=None
        for ids,mat in self.faces:
            if mat!=last:lines.append('usemtl '+mat);last=mat
            lines.append('f '+' '.join('%d/%d'%(idx+1,j+1) for j,idx in enumerate(reversed(ids))))
        (OUT/(self.name+'.obj')).write_text('\n'.join(lines),encoding='utf-8')
        manifest['meshes'].append(dict(name=self.name,collision=collision,nanite=nanite))


def generate():
    OUT.mkdir(parents=True,exist_ok=True)
    m=Mesh('SM_R3D_Pipe_400')
    m.rod((-200,0,0),(200,0,0),24,'Teal',24)
    for x in [-192,192]:
        m.rod((x-5,0,0),(x+5,0,0),32,'Teal',24)
        for i in range(8):
            a=i*math.tau/8;m.rod((x-6,27*math.cos(a),27*math.sin(a)),(x+6,27*math.cos(a),27*math.sin(a)),2.5,'Steel',6)
    m.save()
    m=Mesh('SM_R3D_PipeCorner')
    for i in range(12):
        a,b=i*math.pi/24,(i+1)*math.pi/24
        m.rod((60*math.cos(a),60*math.sin(a),0),(60*math.cos(b),60*math.sin(b),0),24,'Teal',20)
    m.save()
    m=Mesh('SM_R3D_PipeBracket')
    m.box((0,12,0),(65,30,18),'Steel');m.box((0,30,-35),(75,8,90),'Steel')
    for x in [-25,25]:m.rod((x,23,-62),(x,35,-62),4,'Orange',8)
    m.save()
    m=Mesh('SM_R3D_ValveStation')
    m.box((0,0,12),(160,90,24),'Concrete');m.rod((-80,0,70),(80,0,70),26,'Teal',24)
    m.rod((0,0,70),(0,0,140),12,'Steel');m.ring((0,0,145),42,5,'Orange')
    for a in range(0,360,60):m.rod((0,0,145),(38*math.cos(math.radians(a)),38*math.sin(math.radians(a)),145),3,'Orange',8)
    for x in [-55,55]:m.rod((x-4,0,70),(x+4,0,70),34,'Steel',24)
    m.save()
    m=Mesh('SM_R3D_Drain_100');m.box((0,0,-1),(100,80,6),'Steel')
    for x in range(-44,46,11):m.box((x,0,3),(5,68,3),'Coping')
    m.save()
    m=Mesh('SM_R3D_JetNozzle_170')
    m.rod((0,0,-8),(0,0,0),91,'Concrete',40);m.ring((0,0,3),78,7,'Teal',40)
    m.rod((0,0,0),(0,0,2),69,'Steel',40)
    for r in [25,45,62]:m.ring((0,0,4),r,2,'Coping')
    for i in range(12):
        a=i*math.tau/12;m.rod((0,0,4),(66*math.cos(a),66*math.sin(a),4),1.8,'Coping',6)
    m.save()
    m=Mesh('SM_R3D_CargoCrate_100')
    m.box((0,0,0),(92,92,96),'Wood')
    for i in range(5):
        for y in [-47,47]:m.box((-38+i*19,y,0),(17,3,94),'WoodLight' if i%2 else 'Wood')
        for x in [-47,47]:m.box((x,-38+i*19,0),(3,17,94),'WoodLight' if i%2 else 'Wood')
        m.box((-38+i*19,0,49),(17,94,2),'WoodLight')
    for x in [-47,47]:
        for y in [-47,47]:m.box((x,y,0),(6,6,100),'Steel')
    for z in [-47,30,47]:
        for y in [-48,48]:m.box((0,y,z),(100,4,6),'Steel')
        for x in [-48,48]:m.box((x,0,z),(4,100,6),'Steel')
    m.save('box',False)
    # 기존 100cm 큐브와 같은 외접 크기: 액터 변환·이동 컴포넌트를 보존한다.
    m=Mesh('SM_R3D_BridgePlank_100')
    for i in range(4):
        m.box((-37.5+i*25,0,0),(24.2,100,100),'WoodLight' if i%2 else 'Wood')
        for y in [-40,40]:m.rod((-37.5+i*25,y,49),(-37.5+i*25,y,50),1.8,'Steel',8)
    m.save('box',False)
    m=Mesh('SM_R3D_RampDeck_100')
    for i in range(12):m.box((-50+(i+.5)*100/12,0,0),(100/12-.35,100,100),'WoodLight' if i%3 else 'Wood')
    m.save('box')
    m=Mesh('SM_R3D_Turntable_100')
    m.rod((0,0,-50),(0,0,50),50,'Teal',64)
    for i in range(48):
        a,b=i*math.tau/48,(i+1)*math.tau/48
        m.poly([(r*math.cos(t),r*math.sin(t),50.05) for r,t in [(45,a),(50,a),(50,b),(45,b)]],'Yellow' if i%2 else 'Steel')
    m.save('cylinder',False)
    m=Mesh('SM_R3D_SweeperBar_100');m.box((0,0,0),(100,100,100),'Yellow')
    for i in range(12):
        x=-50+i*100/12;m.poly([(x,-50,50.03),(x+4,-50,50.03),(x+8,50,50.03),(x+4,50,50.03)],'Steel')
    m.save('box',False)
    # 화산형 타원 팬으로 메시 자체가 불규칙한 웅덩이 가장자리를 만든다.
    for variant,seed in enumerate([31,57,83]):
        rng=random.Random(seed);m=Mesh('SM_R3D_Puddle_'+str(variant));pts=[]
        for i in range(64):
            a=i*math.tau/64;r=1+.12*math.sin(a*3+seed)+.08*math.sin(a*7+seed*.2)
            pts.append((200*r*math.cos(a),150*r*math.sin(a),0))
        for i in range(64):m.poly([(0,0,0),pts[i],pts[(i+1)%64]],'Water')
        m.save(nanite=False)
    for variant in range(3):
        rng=random.Random(700+variant);m=Mesh('SM_R3D_Cliff_'+str(variant));rings=[];n=10
        for z,rs in [(-900,1.1),(-500,1),(-100,.95),(350,.78),(750,.5)]:
            ring=[]
            for i in range(n):
                a=i*math.tau/n;radius=(420+rng.uniform(-65,65))*rs
                ring.append((radius*math.cos(a)+z*.08,radius*math.sin(a),z+rng.uniform(-45,45)))
            rings.append(ring)
        for j in range(len(rings)-1):
            for i in range(n):
                k=(i+1)%n;m.poly([rings[j][i],rings[j][k],rings[j+1][k],rings[j+1][i]],'Rock')
        m.poly(rings[-1],'Rock');m.poly(list(reversed(rings[0])),'Rock');m.save()
    m=Mesh('SM_R3D_ConcretePanel_100');m.box((0,0,0),(100,100,100),'Concrete');m.save('box')
    # 패널 틈과 배수 홈은 별도 메시로 필요한 구역만 삭제할 수 있게 한다.
    m=Mesh('SM_R3D_ExpansionJoint_400');m.box((0,0,0),(400,1.2,.3),'Steel');m.save()
    (OUT/'round3.mtl').write_text('\n'.join('newmtl %s\nKd %s'%(k,' '.join(map(str,v))) for k,v in PALETTE.items()),encoding='utf-8')
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('Round3 dam meshes:',len(manifest['meshes']))


if __name__=='__main__':generate()
