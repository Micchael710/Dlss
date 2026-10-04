"""Prepare exactly one combined run; historical SR PASS is read, never rerun."""
from pathlib import Path
import datetime, hashlib, json, re, shutil, struct, zipfile
root=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
base=root/'builds/experimental/dlss-sr'
previous=root/'logs/runtime/dlss-sr/20261004-173230-747'
gate=json.loads((previous/'runtime-result.json').read_text())
assert gate['status']=='PASS' and gate['process_exit_code']==0
oldsr=root/'builds/experimental/dlssg-x2/superresolution-dlssg-x2-candidate.jar'
oldw=root/'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar'
src=max((root/'superresolution/neoforge/build/libs').glob('*.opengl.jar'),key=lambda p:p.stat().st_mtime)
wsrc=root/'wisteria/neoforge/build/libs/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1.jar'
native=root/'wisteria/native/build/dlssg-windows-x64/wisteria_dlssg_bridge.dll'
targets=[]
for source,old,prefix in [(src,oldsr,'superresolution'),(wsrc,oldw,'wisteria')]:
    target=base/(prefix+'-sr-x2-handoff-'+sha(source)[:12]+'.jar')
    assert not target.exists(), 'Do not overwrite a prepared candidate'
    with zipfile.ZipFile(source) as new,zipfile.ZipFile(old) as baseline,zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as out:
        for entry in new.infolist():
            out.writestr(entry,new.read(entry.filename))
            if entry.filename.endswith('.class'):
                assert struct.unpack_from('>H',new.read(entry.filename),6)[0]<=65
        for name in baseline.namelist():
            protected=name.startswith('lib/') and name.endswith('.dll') if prefix=='superresolution' else name in ['natives/windows-x64/wisteria_fsr_bridge.dll','natives/windows-x64/amd_fidelityfx_vk.dll']
            if protected:
                if name in new.namelist(): assert baseline.read(name)==new.read(name), 'Protected native changed: '+name
                else: out.writestr(name,baseline.read(name))
        if prefix=='wisteria':
            classes=[n for n in baseline.namelist() if n.startswith('org/ireallywanttosleep/wisteria/fsr/') and n.endswith('.class')]
            assert len(classes)==9
            assert all(new.read(n)==baseline.read(n) for n in classes), 'AMD bytecode changed'
            assert new.read('natives/windows-x64/wisteria_dlssg_bridge.dll')==native.read_bytes()
    targets.append(target)
with zipfile.ZipFile(targets[1]) as final,zipfile.ZipFile(oldw) as baseline:
    protected=classes+['natives/windows-x64/wisteria_fsr_bridge.dll','natives/windows-x64/amd_fidelityfx_vk.dll']
    amd={n:hashlib.sha256(final.read(n)).hexdigest() for n in protected}
    assert all(final.read(n)==baseline.read(n) for n in protected)
out=root/'logs/runtime/dlss-sr'/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')[:-3]+'-handoff-x2')
out.mkdir(parents=True)
component=root/'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
runtime=root/'experiments/ngx-dlssg-probe/vendor/nvngx_dlssg.dll'
assert sha(component)=='c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838'
assert sha(runtime)=='ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82'
for p,folder in [(component,'component'),(runtime,'stock-runtime')]:
    (out/folder).mkdir();shutil.copy2(p,out/folder/p.name)
config=root/'runtime-baseline/instance/config/super_resolution/config.toml'
original=config.read_bytes();(out/'original-config.txt').write_bytes(original)
text=original.decode('utf-8').replace('\r\n','\n').replace('\r','')
for pattern,value in [(r'(?m)^upscale_algo = "[^"]+"','upscale_algo = "dlss"'),(r'(?m)^upscale_ratio = [^\n]+','upscale_ratio = 1.724'),
                     (r'(?m)^(\s*skip_init_vulkan = )\w+',r'\1false'),(r'(?m)^(\s*enable_debug = )\w+',r'\1false')]:
    text,n=re.subn(pattern,value,text);assert n==1
for section,key,value in [('frame_generation','mode','X2'),('frame_generation','provider','wisteria:dlssg_fg'),('frame_generation','backend','wisteria:dlssg'),('presentation','backend','VULKAN')]:
    text,n=re.subn(r'(\['+section+r'\][\s\S]*?'+key+r' = )"[^"]+"',lambda m:m[1]+'"'+value+'"',text,count=1);assert n==1
config.write_bytes(text.encode('utf-8'))
manifest=dict(run_id=out.name,status='PREPARED_NOT_LAUNCHED',attempt_limit=1,standalone_gate=previous.name,standalone_rerun=False,
    sr='dlss',fg='X2',requested_count=1,ratio=1.724,required_runtime_java='25.0.4',bytecode_target=21,
    sr_jar=str(targets[0].relative_to(root)),sr_sha256=sha(targets[0]),wisteria_jar=str(targets[1].relative_to(root)),wisteria_sha256=sha(targets[1]),
    bridge_abi=3,bridge_sha256=sha(native),protected_amd_9_classes_2_dlls_identical=True,amd_hashes=amd,
    component_sha256=sha(component),fg_runtime_sha256=sha(runtime),original_config_sha256=hashlib.sha256(original).hexdigest(),test_config_sha256=sha(config))
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(base/'current-combined-run.txt').write_text(str(out))
print(out);print('Prepared one combined run; AMD 9 classes + 2 DLLs byte-identical; no AMD or standalone execution')
