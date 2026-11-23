import csv,sys
def rd(p): return {(r['type'],r['scene'],r['n'],r['engine']):r for r in csv.DictReader(l for l in open(p) if not l.startswith('#'))}
a=rd(sys.argv[1]); b=rd(sys.argv[2])
for k in sorted(a, key=lambda k:(k[0],k[1],int(k[2]))):
    if k[3]!='index' or k not in b: continue
    x,y=a[k],b[k]; same=x['candidates']==y['candidates'] and x['node_visits']==y['node_visits'] and x['checksum']==y['checksum']
    t,u=float(x['estimate_ns']),float(y['estimate_ns'])
    base=lambda d: min(float(d[(k[0],k[1],k[2],e)]['estimate_ns']) for e in ('sweep','grid'))
    o=lambda d: float(d[(k[0],k[1],k[2],'index-ordered')]['estimate_ns'])
    print(f"{k[0]:6} {k[1][:14]:14} {k[2]:>6} {(u/t-1)*100:+6.1f}%  G2 {t/base(a):5.2f} -> {u/base(b):5.2f}  G9 {o(a)/t:4.2f} -> {o(b)/u:4.2f} ord {(o(b)/o(a)-1)*100:+6.1f}% prep {float(x['prepare_median_ns'])/1e3:8.1f}->{float(y['prepare_median_ns'])/1e3:8.1f} pairs {float(x['pairs_median_ns'])/1e3:8.1f}->{float(y['pairs_median_ns'])/1e3:8.1f} {'' if same else 'DIFF'}")
