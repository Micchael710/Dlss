"""Stage only supplied x64 SDK binaries and the existing historical SM86 loader layout."""
from pathlib import Path
import hashlib, json, shutil, struct, zipfile, sys

root=Path(__file__).resolve().parents[2]
sdk=Path(sys.argv[1]).resolve()
runtime=root/'experiments/streamline-mfg-capability/runtime'
logs=root/'logs/research/streamline-mfg-capability/20261004-sdk212'
assert not runtime.exists(), 'Runtime already staged; no automatic repeat'
runtime.mkdir();logs.mkdir(parents=True,exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
names=['sl.interposer.dll','sl.common.dll','sl.dlss_g.dll','sl.reflex.dll','sl.pcl.dll','nvngx_dlssg.dll','NvLowLatencyVk.dll']
files=[]
with zipfile.ZipFile(sdk) as z:
    local=root/'superresolution/native/cpp/third_party/Streamline/include'
    checked=[]
    for h in sorted(local.glob('*.h')):
        if 'include/'+h.name in z.namelist():
            assert h.read_bytes().replace(b'\r\n',b'\n')==z.read('include/'+h.name).replace(b'\r\n',b'\n'), 'SDK header mismatch '+h.name
            checked.append(h.name)
    assert {'sl_version.h','sl_dlss_g.h','sl_core_api.h'} <= set(checked)
    for name in names:
        entry='bin/x64/'+name;data=z.read(entry)
        pe=struct.unpack_from('<I',data,0x3c)[0];assert data[pe:pe+4]==b'PE\0\0'
        machine=struct.unpack_from('<H',data,pe+4)[0];assert machine==0x8664, 'Not AMD64 '+entry
        (runtime/name).write_bytes(data)
        files.append(dict(name=name,archive_entry=entry,sha256=sha(data),pe_machine=hex(machine)))
component=root/'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
assert sha(component.read_bytes())=='c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838'
(runtime/'component').mkdir();shutil.copy2(component,runtime/'component/version.dll')
# Same public INI and loader layout as ExternalLoader; ceiling is NOT a queried maximum.
# Preserve historical SpoofArchToGame=0; this harness adds no architecture spoof.
config=f'''[General]
Enabled=1
[FrameGeneration]
Optimized=0
MaxGeneratedFrames=4
[Compatibility]
Router=SM86
KernelImage=Auto
SpoofArchToGame=0
[Logging]
Level=3
Directory={logs/'backend'}
[Runtime]
Mode=Bundled
CacheDirectory={runtime/'bundle-cache'}
'''
(runtime/'component/dlssg_sm86.ini').write_text(config)
(logs/'sm86-config.txt').write_text(config)
files.append(dict(name='component/version.dll',version='0.3.5',sha256=sha(component.read_bytes())))
report=dict(package=str(sdk),package_sha256=sha(sdk.read_bytes()),sdk_version='2.12.0',header_matches=checked,
    files=files,config_max_generated_frames=4,config_is_not_runtime_capability=True,
    loader_reference='experiments/dlssg-external-harness/harness.cpp:ExternalLoader',runtime=str(runtime))
(logs/'staged-sdk.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'headers_matched':len(checked),'dlls_staged':len(files),'runtime':str(runtime)}))
