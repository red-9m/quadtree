import csv,sys
def rd(p): return {(r['scene'],r['n'],r['engine']):r for r in csv.DictReader(l for l in open(p) if not l.startswith('#'))}
a=rd(sys.argv[1]); b=rd(sys.argv[2])
for k in sorted(a, key=lambda k:(k[0],int(k[1]),k[2])):
    if k[2]!='index' and not (len(sys.argv)>3 and k[2]=='index-ordered'): continue
    x,y=a[k],b[k]
    same = x['candidates']==y['candidates'] and x['node_visits']==y['node_visits'] and x['checksum']==y['checksum']
    t,u=float(x['estimate_ns']),float(y['estimate_ns'])
    base=lambda d: min(float(d[(k[0],k[1],e)]['estimate_ns']) for e in ('sweep','grid'))
    print(f"{k[0][:14]:14} {k[1]:>6} {k[2][:9]:9} {t/1e3:10.1f} -> {u/1e3:10.1f} us ({(u/t-1)*100:+6.1f}%) ratio {t/base(a):5.2f} -> {u/base(b):5.2f} prep {float(x['prepare_median_ns'])/1e3:8.1f}->{float(y['prepare_median_ns'])/1e3:8.1f} {'' if same else 'COUNTS DIFFER'}")
