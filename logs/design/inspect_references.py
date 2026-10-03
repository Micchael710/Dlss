import zipfile, pathlib, re, hashlib, json, struct
root = pathlib.Path('D:/ProjectDllsss')
refs = pathlib.Path('C:/Users/micha/AppData/Roaming/.minecraft/versions/Apparatia Alpha/mods')
report = {}
for p in refs.glob('*.jar'):
    if not p.name.startswith(('wisteria-', 'super_resolution-')): continue
    with zipfile.ZipFile(p) as z:
        selected = [n for n in z.namelist() if any(s in n for s in ('framegeneration','FrameGeneration','backend/fsr','WisteriaNative','LICENSE','NOTICE','jarjar/metadata'))]
        item = {'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'selected_entries': selected}
        for n in selected:
            if n.endswith('.dll'):
                data = z.read(n)
                strings = [m.decode('ascii') for m in re.findall(rb'[ -~]{8,}', data)]
                item[n] = {'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'relevant_strings':[s for s in strings if any(k.lower() in s.lower() for k in ('fsr','ffx','optical','shader','dispatch','glsl','mix(','imageStore','lerp','Fidelity','interpol'))]}
        report[p.name] = item
for p in (root/'builds/baseline').glob('*.jar'):
    with zipfile.ZipFile(p) as z:
        report['baseline:'+p.name] = {'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'embedded_jars':[n for n in z.namelist() if n.endswith('.jar')], 'notices':[n for n in z.namelist() if 'license' in n.lower() or 'notice' in n.lower()], 'metadata':{n:z.read(n).decode('utf-8') for n in z.namelist() if n.endswith('jarjar/metadata.json')}}
(root/'logs/design/reference-static-inventory.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
for k,v in report.items():
    print(k, v['sha256'])
    for n,x in v.items():
        if n.endswith('.dll'): print(n,json.dumps(x,ensure_ascii=False)[:12000])
    if k.startswith('baseline:'): print(v)
