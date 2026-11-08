"""Round2 보행면에서 열린 펜스 경계를 찾고 교체 가능한 블록아웃 메시를 만든다."""
import collections
import json
import math


def height_at(triangle, x, y):
    a, b, c = triangle
    d = (b[1]-c[1])*(a[0]-c[0]) + (c[0]-b[0])*(a[1]-c[1])
    if abs(d) < .001:
        return None
    u = ((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/d
    v = ((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/d
    w = 1-u-v
    return u*a[2]+v*b[2]+w*c[2] if min(u, v, w) >= -.001 else None


class Mesh:
    def __init__(self, name):
        self.name = name
        self.points, self.faces = [], []

    def face(self, points):
        n = len(self.points)
        self.points.extend(points)
        for i in range(1, len(points)-1):
            self.faces.append((n, n+i, n+i+1))

    def box(self, center, size):
        x, y, z = center
        u, v, w = [d/2 for d in size]
        p = [(x-u,y-v,z-w),(x+u,y-v,z-w),(x+u,y+v,z-w),(x-u,y+v,z-w),
             (x-u,y-v,z+w),(x+u,y-v,z+w),(x+u,y+v,z+w),(x-u,y+v,z+w)]
        for ids in [(3,2,1,0),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,6,7)]:
            self.face([p[i] for i in ids])

    def rod(self, a, b, radius, sides=6):
        d = [b[i]-a[i] for i in range(3)]
        length = math.sqrt(sum(v*v for v in d))
        if length < .01:
            return
        d = [v/length for v in d]
        q = [0,0,1] if abs(d[2]) < .9 else [1,0,0]
        u = [d[1]*q[2]-d[2]*q[1],d[2]*q[0]-d[0]*q[2],d[0]*q[1]-d[1]*q[0]]
        length = math.sqrt(sum(v*v for v in u))
        u = [v/length for v in u]
        v = [d[1]*u[2]-d[2]*u[1],d[2]*u[0]-d[0]*u[2],d[0]*u[1]-d[1]*u[0]]
        rings = [[[p[j]+radius*(u[j]*math.cos(i*math.tau/sides)+v[j]*math.sin(i*math.tau/sides))
                   for j in range(3)] for i in range(sides)] for p in (a,b)]
        self.face(list(reversed(rings[0])))
        self.face(rings[1])
        for i in range(sides):
            k = (i+1)%sides
            self.face([rings[0][i],rings[0][k],rings[1][k],rings[1][i]])

    def save(self, folder):
        path = folder/(self.name+'.obj')
        lines = ['o '+self.name, 'mtllib organic.mtl', 'usemtl Neutral']
        lines += ['v %.5f %.5f %.5f'%(x,-y,z) for x,y,z in self.points]
        lines += ['vt 0 0','vt 1 0','vt 1 1']
        lines += ['f '+' '.join('%d/%d'%(i+1,k+1) for k,i in enumerate(reversed(f))) for f in self.faces]
        path.write_text('\n'.join(lines), encoding='utf-8')
        return path


def make_sources(triangles, folder):
    folder.mkdir(parents=True, exist_ok=True)
    (folder/'organic.mtl').write_text('newmtl Neutral\nKd .3 .32 .34\n')
    # 장식 봉우리·점프대는 발판 가장자리와 구분한다. 원본 승강판에는 고정 펜스를 붙이지 않는다.
    selected = [t for t in triangles if t['actor'].startswith('SM_Dummy_') and not
                any(s in t['actor'] for s in ['Rubble','GroundMound','Cylinder'])]
    all_surfaces = [t for t in triangles if not t['actor'].startswith('BP_JumpPad')]
    boundaries = collections.defaultdict(list)
    for t in selected:
        p = t['p']
        for i in range(3):
            a,b = p[i],p[(i+1)%3]
            key = tuple(sorted(tuple(round(v,1) for v in q) for q in (a,b)))
            boundaries[(t['actor'],key)].append((a,b,p[(i+2)%3]))
    segments = []
    for (actor,_),edges in boundaries.items():
        if len(edges) != 1:
            continue
        a,b,c = edges[0]
        dx,dy = b[0]-a[0],b[1]-a[1]
        length = math.hypot(dx,dy)
        if length < 8:
            continue
        normal = [-dy/length,dx/length]
        if sum(normal[k]*(c[k]-(a[k]+b[k])/2) for k in range(2)) > 0:
            normal = [-v for v in normal]
        count = max(1,math.ceil(length/65))
        accepted = []
        for i in range(count):
            t = (i+.5)/count
            p = [a[k]+(b[k]-a[k])*t for k in range(3)]
            inside = [p[k]-normal[k]*20 for k in range(2)]
            outside = [p[k]+normal[k]*35 for k in range(2)]
            blocked,connected = False,False
            for surface in all_surfaces:
                h = height_at(surface['p'],*inside)
                if h is not None and h > p[2]+40 and surface['actor'].startswith('SM_Dummy_Block'):
                    blocked = True
                h = height_at(surface['p'],*outside)
                if h is None:
                    continue
                # 기존 계단 턱·짧은 점프 연결과 높은 땅의 옆벽은 가로막지 않는다.
                if abs(h-p[2]) <= 260 or (h > p[2] and surface['actor'].startswith('SM_Dummy_Block')):
                    connected = True
            # 북쪽으로 승하차하는 기존 엘리베이터의 폭을 유지한다.
            lift_port = -2795 < p[0] < -1570 and abs(p[1]+1164) < 90 and 900 < p[2] < 2600
            accepted.append(not(blocked or connected or lift_port))
        # 짧은 판자와 계단 출입구를 표본 간격보다 넓게 비워 캐릭터가 통과할 여유를 둔다.
        widened = accepted[:]
        for i in range(1,count-1):
            if not accepted[i] and (accepted[i-1] or accepted[i+1]):
                widened[max(0,i-1):min(count,i+2)] = [False]*len(widened[max(0,i-1):min(count,i+2)])
        accepted = widened
        start = None
        for i,ok in enumerate(accepted+[False]):
            if ok and start is None:
                start = i
            if not ok and start is not None:
                aa = [a[k]+(b[k]-a[k])*start/count for k in range(3)]
                bb = [a[k]+(b[k]-a[k])*i/count for k in range(3)]
                if aa[:2] > bb[:2]:
                    aa,bb = bb,aa
                group = 'Stairs' if 'Stairs' in actor else ('Hub' if 'Wall' in actor or 'Window' in actor else 'Outer')
                segments.append({'a':aa,'b':bb,'normal':normal,'group':group,'actor':actor})
                start = None
    # 인접한 계단 측면을 합쳐 계단마다 짧은 기둥이 생기지 않게 한다.
    changed = True
    while changed:
        changed = False
        for i,s in enumerate(segments):
            for j in range(i+1,len(segments)):
                t = segments[j]
                if s['group'] != t['group'] or sum(s['normal'][k]*t['normal'][k] for k in range(2)) < .999:
                    continue
                if math.dist(s['b'][:2],t['a'][:2]) < 2 and abs(s['b'][2]-t['a'][2]) < 50:
                    s['b'] = t['b'];segments.pop(j);changed = True;break
                if math.dist(t['b'][:2],s['a'][:2]) < 2 and abs(t['b'][2]-s['a'][2]) < 50:
                    s['a'] = t['a'];segments.pop(j);changed = True;break
            if changed:
                break
    meshes = {g:Mesh('SM_Round2_OpenFence_'+g) for g in ['Outer','Hub','Stairs','Lift']}
    for s in segments:
        a,b = s['a'],s['b'];m = meshes[s['group']]
        n = max(1,math.ceil(math.dist(a[:2],b[:2])/240))
        for i in range(n+1):
            p = [a[k]+(b[k]-a[k])*i/n for k in range(3)]
            m.rod(p,[p[0],p[1],p[2]+360],6)
        for z in [35,180,360]:
            m.rod([a[0],a[1],a[2]+z],[b[0],b[1],b[2]+z],3.5)
        n = max(1,math.ceil(math.dist(a[:2],b[:2])/100))
        for i in range(n+1):
            p = [a[k]+(b[k]-a[k])*i/n for k in range(3)]
            m.rod([p[0],p[1],p[2]+35],[p[0],p[1],p[2]+360],1.2,4)
        for z in [100,260]:
            m.rod([a[0],a[1],a[2]+z],[b[0],b[1],b[2]+z],1.2,4)
    # 승강판의 진행 경로 바깥 세 면을 그물 구조로 감싼다. 북쪽 승하차면은 비운다.
    m = meshes['Lift']
    for x,y in [(-2810,-2430),(-1555,-2430),(-2810,-1210),(-1555,-1210)]:
        m.rod((x,y,965),(x,y,2920),7)
    for a,b in [((-2810,-2430),(-1555,-2430)),((-2810,-2430),(-2810,-1210)),((-1555,-2430),(-1555,-1210))]:
        n = math.ceil(math.dist(a,b)/130)
        for z in range(1000,2921,240):
            m.rod((*a,z),(*b,z),2)
        for i in range(n+1):
            x,y = [a[k]+(b[k]-a[k])*i/n for k in range(2)]
            m.rod((x,y,965),(x,y,2920),1.2,4)
    sweeper = Mesh('SM_Round2_SweepBar')
    # 기존 회전판 자식의 (10,10,.2) 스케일에서 800×70×100cm가 되도록 굽는다.
    sweeper.box((0,0,0),(80,7,500))
    slider = Mesh('SM_Round2_Slider')
    # 기존 상하 발판 자식 (2,2,.2)과 (-100,-100,0)을 유지해 200×200×130cm로 만든다.
    slider.box((50,50,325),(100,100,650))
    for mesh in list(meshes.values())+[sweeper,slider]:
        mesh.save(folder)
    return {'segments':segments,'meshes':[m.name for m in list(meshes.values())+[sweeper,slider]],
            'fence_height':360,'collision_height':500}
