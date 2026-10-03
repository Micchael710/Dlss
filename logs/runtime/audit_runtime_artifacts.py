"""Read-only artifact audit and minimal diagnostic export; never loads native code."""
import hashlib, json, pathlib, re, zipfile
root = pathlib.Path('D:/ProjectDllsss')
records = []
for p in sorted((root/'builds/baseline').glob('*.jar')):
    with zipfile.ZipFile(p) as archive:
        names = archive.namelist()
        records.append({'file':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
                        'native_entries':[n for n in names if n.lower().endswith(('.dll','.so','.dylib'))],
                        'required_sr_core_present':'lib/libSuperResolution+win64+release.dll' in names,
                        'legacy_wisteria_native_present':any('WisteriaNative' in n for n in names)})
(root/'logs/runtime/runtime-artifact-audit.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
logdir=root/'logs/runtime'
for p in list(logdir.glob('*.log')):
    content=p.read_text(encoding='utf-8-sig',errors='replace')
    content=re.sub(r'C:[\\/]Users[\\/]micha', '<USER_HOME>',content,flags=re.I)
    content=content.replace('lwjgl_micha','lwjgl_TEST_USER')
    content=re.sub(r'(--accessToken(?:,|\s|=)+)[^\s,\]]+',r'\1<REDACTED>',content)
    p.write_text(content,encoding='utf-8')
latest=(root/'runtime-baseline/instance/logs/latest.log').read_text(encoding='utf-8',errors='replace')
latest=re.sub(r'C:[\\/]Users[\\/]micha','<USER_HOME>',latest,flags=re.I)
lines=latest.splitlines()
errors=[]
for i,line in enumerate(lines):
    if any(term in line for term in ('/ERROR]', '/FATAL]', 'Required dependency extraction failed')):
        errors.append(f'latest.log:{i+1}: {line}')
(logdir/'baseline-errors.log').write_text('\n'.join(errors)+'\n',encoding='utf-8')
(logdir/'baseline-game.log').write_text(latest,encoding='utf-8')
print(json.dumps(records,indent=2))
print('Error records:',len(errors))
