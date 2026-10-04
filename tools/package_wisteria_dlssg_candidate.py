"""Local candidates only; preserve baseline natives and prove AMD classes unchanged."""
from pathlib import Path
import hashlib,json,zipfile,struct
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'builds/baseline'
OUT=ROOT/'builds/experimental/dlssg-x2'
OUT.mkdir(parents=True,exist_ok=True)
def sha(data):return hashlib.sha256(data).hexdigest()
srbase=BASE/'super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.f0e41bf8.opengl-fg-backpressure.jar'
wbase=BASE/'wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar'
expected={srbase:'20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896',
          wbase:'ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53',
          ROOT/'wisteria/native/build/fsr-windows-x64/wisteria_fsr_bridge.dll':'b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b'}
for p,digest in expected.items():assert sha(p.read_bytes())==digest,p
sr=max((ROOT/'superresolution/neoforge/build/libs').glob('*.opengl.jar'),key=lambda p:p.stat().st_mtime)
w=ROOT/'wisteria/neoforge/build/libs/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1.jar'
manifest={'baseline_unchanged':True,'baseline_sha256':{str(p.relative_to(ROOT)):v for p,v in expected.items()},'artifacts':{}}
for label,source,baseline in [('superresolution',sr,srbase),('wisteria',w,wbase)]:
    target=OUT/(label+'-dlssg-x2-candidate.jar')
    with zipfile.ZipFile(source) as src,zipfile.ZipFile(baseline) as old,zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as dst:
        assert src.testzip() is None
        names=set(src.namelist())
        for info in src.infolist():dst.writestr(info,src.read(info.filename))
        # SR packaging already validated this DLL; do not rebuild or replace it.
        for name in old.namelist():
            if name.startswith('lib/') and name.endswith('.dll') and name not in names:dst.writestr(name,old.read(name))
        if label=='wisteria':
            fsr=[n for n in old.namelist() if n.startswith('org/ireallywanttosleep/wisteria/fsr/') and n.endswith('.class')]
            for n in fsr:assert src.read(n)==old.read(n),'AMD bytecode changed: '+n
            for n in ['natives/windows-x64/wisteria_fsr_bridge.dll','natives/windows-x64/amd_fidelityfx_vk.dll']:
                assert src.read(n)==old.read(n),'AMD native changed: '+n
            assert 'natives/windows-x64/wisteria_dlssg_bridge.dll' in names
            assert not any('sm86_backend' in n or n.endswith('/version.dll') for n in names)
            manifest['amd_classes_unchanged']=len(fsr)
    with zipfile.ZipFile(target) as z:
        own=[n for n in z.namelist() if n.endswith('.class') and n.startswith(('org/ireallywanttosleep/wisteria/','io/homo/superresolution/'))]
        assert all(struct.unpack_from('>H',z.read(n),6)[0]==65 for n in own)
        manifest['artifacts'][label]={'path':str(target.relative_to(ROOT)), 'sha256':sha(target.read_bytes()),'java21_classes':len(own),'bytes':target.stat().st_size}
manifest['transport']='four Vulkan GPU blits; zero CPU image transport; direct imported G1 lease'
(OUT/'artifact-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(ROOT/'logs/runtime/dlssg-integration-artifact-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print(json.dumps(manifest,indent=2))
