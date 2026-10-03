"""Inspect supplied archives without loading native code or executing builds."""
import hashlib,json,struct,zipfile
from pathlib import Path
root=Path('D:/ProjectDlssmiAmigo')
rows=[]
for jar in sorted((root/'build_jars').glob('*.jar')):
    with zipfile.ZipFile(jar) as z:
        native=[];versions={};classes=0
        for n in z.namelist():
            if n.endswith(('.dll','.so')):
                d=z.read(n);native.append({'path':n,'bytes':len(d),'sha256':hashlib.sha256(d).hexdigest()})
            if n.endswith('.class') and n.startswith(('org/ireallywanttosleep/wisteria/','io/homo/superresolution/')):
                d=z.read(n);major=struct.unpack('>H',d[6:8])[0];versions[major]=versions.get(major,0)+1;classes+=1
        rows.append({'path':str(jar),'bytes':jar.stat().st_size,'sha256':hashlib.sha256(jar.read_bytes()).hexdigest(),'native_libraries':native,'own_classes':classes,'class_major_counts':versions})
target=Path(__file__).resolve().parents[1]/'logs/research/friend-comparison/artifacts.json'
target.write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps(rows,indent=2))
