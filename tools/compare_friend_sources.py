"""Read-only comparison; save inventories/diffs only inside our research folder."""
import hashlib,json,difflib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
friend=Path('D:/ProjectDlssmiAmigo')
dest=root/'logs/research/friend-comparison'
dest.mkdir(parents=True,exist_ok=True)
skip={'.git','.gradle','build','buildLinux','buildWindows','bin','out','third_party','node_modules','.idea','versions','logs'}
extensions={'.java','.kt','.kts','.cpp','.h','.comp','.glsl','.frag','.vert','.json','.toml','.properties'}
def inventory(base):
    result={}
    for p in base.rglob('*'):
        rel=p.relative_to(base)
        if not p.is_file() or any(x in skip for x in rel.parts) or p.suffix not in extensions:continue
        result[rel.as_posix()]=p
    return result
summary={}
for ours,theirs in [('superresolution','superresolution'),('wisteria','Wisteria')]:
    a,b=inventory(root/ours),inventory(friend/theirs)
    rows=[]
    for name in sorted(a.keys()|b.keys()):
        status='friend_only' if name not in a else 'ours_only' if name not in b else 'identical' if a[name].read_bytes()==b[name].read_bytes() else 'different'
        rows.append({'path':name,'status':status})
        if status=='different':
            ta=a[name].read_text(encoding='utf-8-sig',errors='replace').splitlines(keepends=True)
            tb=b[name].read_text(encoding='utf-8-sig',errors='replace').splitlines(keepends=True)
            path=dest/'diffs'/ours/(name+'.diff');path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(''.join(difflib.unified_diff(ta,tb,fromfile='ours/'+name,tofile='friend/'+name)),encoding='utf-8')
    summary[ours]={'ours_count':len(a),'friend_count':len(b),'counts':{s:sum(r['status']==s for r in rows) for s in ['identical','different','ours_only','friend_only']},'files':rows}
(dest/'inventory.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
for repo,data in summary.items():
    print(repo,json.dumps(data['counts']))
    for row in data['files']:
        if row['status']!='identical':print(row['status'],row['path'])
