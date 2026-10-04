"""Package existing DLSS option and one FG-off isolated run; no capability fabrication."""
from pathlib import Path
import datetime,hashlib,json,shutil,struct,zipfile,re
root=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
out=root/'logs/runtime/dlss-sr'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')[:-3]
out.mkdir(parents=True)
base=root/'builds/experimental/dlss-sr';base.mkdir(exist_ok=True)
old=root/'builds/experimental/dlssg-x2/superresolution-dlssg-x2-candidate.jar'
source=max((root/'superresolution/neoforge/build/libs').glob('*.opengl.jar'),key=lambda p:p.stat().st_mtime)
native=root/'builds/experimental/dlss-sr/native/libSuperResolutionNGX+win64+release.dll'
target=base/'superresolution-dlss-sr-candidate.jar'
reuse=target.exists()
with zipfile.ZipFile(old) as previous,zipfile.ZipFile(source) as new,zipfile.ZipFile(target,'r' if reuse else 'w',zipfile.ZIP_DEFLATED) as dest:
    for i in new.infolist():
        if reuse:assert dest.read(i.filename)==new.read(i.filename),'Prepared artifact differs from built source'
        else:dest.writestr(i,new.read(i.filename))
    for n in previous.namelist():
        if n.startswith('lib/') and n.endswith('.dll'):
            if n in new.namelist():assert previous.read(n)==new.read(n),'Protected native changed: '+n
            elif reuse:assert dest.read(n)==previous.read(n)
            else:dest.writestr(n,previous.read(n))
    name='lib/libSuperResolutionNGX+win64+release.dll'
    assert new.read(name)==native.read_bytes(),'Missing/stale own NGX JNI binding'
    for n in new.namelist():
        if n.endswith('.class') and n.startswith('io/homo/superresolution/'):
            assert struct.unpack_from('>H',new.read(n),6)[0]==65
    # Frame generation and presentation classes remain unchanged.
    for n in previous.namelist():
        if n.endswith('.class') and n.startswith(('io/homo/superresolution/common/framegeneration/','io/homo/superresolution/common/presentation/')):
            assert previous.read(n)==new.read(n),'FG/presentation class changed: '+n
wisteria=root/'builds/experimental/dlssg-x2/wisteria-dlssg-x2-candidate.jar'
wbase=root/'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar'
with zipfile.ZipFile(wisteria) as new,zipfile.ZipFile(wbase) as oldw:
    protected=[n for n in oldw.namelist() if n.startswith('org/ireallywanttosleep/wisteria/fsr/') and n.endswith('.class')]
    protected+=['natives/windows-x64/wisteria_fsr_bridge.dll','natives/windows-x64/amd_fidelityfx_vk.dll']
    assert len(protected)==11 and all(new.read(n)==oldw.read(n) for n in protected)
config=root/'runtime-baseline/instance/config/super_resolution/config.toml'
original=config.read_bytes();(out/'original-config.txt').write_bytes(original)
text=original.decode('utf-8').replace('\r\n','\n').replace('\r','');text,n=re.subn(r'(?m)^upscale_algo = "[^"\r\n]+"\r?$','upscale_algo = "dlss"',text);assert n==1
text,n=re.subn(r'(?m)^upscale_ratio = [^\r\n]+\r?$', 'upscale_ratio = 1.724',text);assert n==1
text,n=re.subn(r'(\[frame_generation\][\s\S]*?mode = )"[^"]+"',r'\1"OFF"',text,count=1);assert n==1
text,n=re.subn(r'(\[presentation\][\s\S]*?backend = )"[^"]+"',r'\1"VULKAN"',text,count=1);assert n==1
config.write_bytes(text.encode('utf-8'))
vendor=base/'official-runtime/nvngx_dlss.dll';libraries=root/'runtime-baseline/instance/config/super_resolution/libraries';libraries.mkdir(exist_ok=True)
existing=libraries/vendor.name
if existing.exists():shutil.copy2(existing,out/'previous-nvngx_dlss.dll')
shutil.copy2(vendor,existing)
manifest=dict(run_id=out.name,attempt_limit=1,status='PREPARED_NOT_LAUNCHED',sr='dlss',fg='OFF',ratio=1.724,
    sr_jar=str(target.relative_to(root)),sr_sha256=sha(target),wisteria_sha256=sha(wisteria),
    own_native_sha256=sha(native),own_native_resource=name,official_runtime=json.loads((base/'official-runtime/manifest.json').read_text(encoding='utf-8-sig')),
    protected_amd_9_classes_2_dlls_identical=True,fg_presentation_bytecode_unchanged=True,previous_mfg_sr_jar_sha256=sha(root/'builds/experimental/dlssg-x2/superresolution-dlssg-x2-candidate.jar'),
    source=str(source.relative_to(root)),required_runtime_java='25.0.4',bytecode_target=21,original_config_sha256=hashlib.sha256(original).hexdigest(),test_config_sha256=sha(config))
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(base/'current-run.txt').write_text(str(out))
print(out);print(json.dumps(manifest,indent=2))
