# Perfboard layout for OpenTherm adapter (Ihor Melnyk circuit). Base coords (c,r); offset applied at render.
OX,OY=4,1
comps = {
 # name: (type, {pin:(c,r)}, value, extra)
 'J1':('ark',{'1':(0,6),'2':(0,8)},'ARK 2-pin',{}),
 'D3':('diode',{'A':(3,6),'K':(3,2)},'1N4148',{}),
 'D4':('diode',{'A':(5,8),'K':(5,4)},'1N4148',{}),
 'D2':('diode',{'K':(2,6),'A':(2,10)},'1N4148',{}),
 'D1':('diode',{'K':(4,8),'A':(4,12)},'1N4148',{}),
 'D5':('zener',{'K':(7,6),'A':(7,10)},'4,7 V',{}),
 'D6':('zener',{'K':(13,8),'A':(13,12)},'15 V',{}),
 'D7':('zener',{'K':(14,7),'A':(14,12)},'4,3 V',{}),
 'Q1':('to92',{'C':(8,5),'B':(9,5),'E':(10,5)},'BC557B',{}),
 'R1':('res',{'1':(8,6),'2':(8,10)},'220 Ω',{}),
 'R2':('res',{'1':(8,12),'2':(8,16)},'100 Ω',{}),
 'R3':('res',{'1':(12,4),'2':(12,8)},'330 Ω',{}),
 'R5':('res',{'1':(19,4),'2':(19,8)},'330 Ω',{}),
 'R4':('res',{'1':(22,12),'2':(22,16)},'1,5 kΩ',{}),
 'R6':('res',{'1':(21,4),'2':(21,1)},'1 kΩ',{}),
 'R7':('res',{'1':(19,12),'2':(19,15)},'1 kΩ',{}),
 'LD1':('led',{'A':(22,1),'K':(23,1)},'czerwona TX',{'color':'red'}),
 'LD2':('led',{'A':(20,15),'K':(21,15)},'zielona RX',{'color':'green'}),
 'U1':('dip4',{'3':(15,7),'4':(15,8),'2':(18,7),'1':(18,8)},'PC817',{'rot':180}),
 'U2':('dip4',{'1':(15,11),'2':(15,12),'3':(18,12),'4':(18,11)},'PC817',{'rot':0}),
 'J2':('header',{'1':(23,3),'2':(23,4),'3':(23,5),'4':(23,6)},'goldpin 1×4',{'labels':{'1':'GND','2':'GPIO22','3':'3V3','4':'GPIO21'}}),
}
traces = [
 ('OT1',(0,6),(3,6)),('OT2',(0,8),(5,8)),
 ('P',(3,0),(12,0)),('P',(3,0),(3,2)),('P',(5,0),(5,4)),('P',(10,0),(10,5)),('P',(12,0),(12,4)),
 ('N',(2,16),(15,16)),('N',(2,10),(2,16)),('N',(4,12),(4,16)),('N',(7,10),(7,16)),('N',(13,12),(13,16)),('N',(14,12),(14,16)),('N',(15,12),(15,16)),
 ('C',(8,5),(8,6)),('C',(7,6),(8,6)),
 ('M',(9,5),(9,8)),('M',(9,8),(15,8)),
 ('U1E',(14,7),(15,7)),
 ('S',(8,10),(8,12)),('S',(8,11),(15,11)),
 ('LED1',(18,8),(19,8)),('TX',(19,4),(23,4)),
 ('GND',(18,0),(18,7)),('GND',(18,0),(24,0)),('GND',(24,0),(24,16)),('GND',(23,3),(24,3)),('GND',(23,0),(23,1)),('GND',(19,16),(24,16)),('GND',(21,15),(21,16)),
 ('LEDR',(21,1),(22,1)),('LEDG',(19,15),(20,15)),
 ('3V3',(18,11),(20,11)),('3V3',(20,5),(20,11)),('3V3',(20,5),(23,5)),
 ('RX',(18,12),(22,12)),('RX',(21,6),(21,12)),('RX',(21,6),(23,6)),
]
expected = {
 'OT1':{'J1.1','D3.A','D2.K'},'OT2':{'J1.2','D4.A','D1.K'},
 'P':{'D3.K','D4.K','Q1.E','R3.1'},
 'N':{'D1.A','D2.A','D5.A','D6.A','D7.A','R2.2','U2.2'},
 'M':{'Q1.B','R3.2','D6.K','U1.4'},'U1E':{'U1.3','D7.K'},
 'C':{'Q1.C','R1.1','D5.K'},'S':{'R1.2','R2.1','U2.1'},
 'LED1':{'R5.2','U1.1'},'TX':{'R5.1','R6.1','J2.2'},
 'GND':{'U1.2','R4.2','J2.1','LD1.K','LD2.K'},'3V3':{'U2.4','J2.3'},'RX':{'U2.3','R4.1','R7.1','J2.4'},
 'LEDR':{'R6.2','LD1.A'},'LEDG':{'R7.2','LD2.A'},
}
def holes(a,b):
    (c1,r1),(c2,r2)=a,b
    assert c1==c2 or r1==r2, (a,b)
    if c1==c2: return [(c1,r) for r in range(min(r1,r2),max(r1,r2)+1)]
    return [(c,r1) for c in range(min(c1,c2),max(c1,c2)+1)]
# union-find over holes
par={}
def f(x):
    par.setdefault(x,x)
    while par[x]!=x: par[x]=par[par[x]]; x=par[x]
    return x
def u(a,b): par[f(a)]=f(b)
pinat={}
for n,(t,pins,v,e) in comps.items():
    for p,h in pins.items():
        assert h not in pinat, ('two pins in one hole',h,pinat.get(h),n+'.'+p)
        pinat[h]=n+'.'+p; f(h)
for net,a,b in traces:
    hs=holes(a,b)
    for h in hs: f(h)
    for x,y in zip(hs,hs[1:]): u(x,y)
groups={}
for h,pn in pinat.items(): groups.setdefault(f(h),set()).add(pn)
got=[g for g in groups.values() if len(g)>1]
ok=True
exp_sets=[frozenset(s) for s in expected.values()]
for g in got:
    if frozenset(g) not in exp_sets: print('UNEXPECTED NET', sorted(g)); ok=False
for name,s in expected.items():
    if not any(frozenset(g)==frozenset(s) for g in got): print('MISSING NET',name,sorted(s)); ok=False
single=[pn for g in groups.values() if len(g)==1 for pn in g]
print('unconnected pins:',single)
# body overlap check: list holes covered by each component body (between pins, straight 2-pin parts)
body={}
for n,(t,pins,v,e) in comps.items():
    if t in('diode','zener','res','led'):
        a,b=list(pins.values())
        for h in holes(a,b): body.setdefault(h,[]).append(n)
    elif t=='dip4':
        cs=[p[0] for p in pins.values()]; rs=[p[1] for p in pins.values()]
        for c in range(min(cs),max(cs)+1):
            for r in range(min(rs),max(rs)+1): body.setdefault((c,r),[]).append(n)
    elif t in('to92','header'):
        for h in pins.values(): body.setdefault(h,[]).append(n)
clash=[(h,v) for h,v in body.items() if len(v)>1]
print('body clashes:',clash)
# traces must not enter opto gap cols 16-17
gap=[(net,a,b) for net,a,b in traces if any(16<=h[0]<=17 for h in holes(a,b))]
print('traces in isolation gap:',gap)
print('NETLIST OK' if ok and not single and not clash and not gap else 'PROBLEMS')
