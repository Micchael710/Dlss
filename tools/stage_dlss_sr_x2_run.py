"""One combined x2 run, gated on this iteration's successful SR standalone."""
from pathlib import Path
import sys,json,datetime,hashlib,shutil,tomllib,re
root=Path(__file__).resolve().parents[1]
previous=Path(sys.argv[1]).resolve();assert previous.is_relative_to(root/'logs/runtime/dlss-sr')
gate=json.loads((previous/'runtime-result.json').read_text());assert gate['status']=='PASS' and gate['process_exit_code']==0
sr=json.loads((previous/'manifest.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(root/sr['sr_jar'])==sr['sr_sha256']
out=root/'logs/runtime/dlss-sr'/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')[:-3]+'-x2');out.mkdir(parents=True)
component=root/'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
runtime=root/'experiments/ngx-dlssg-probe/vendor/nvngx_dlssg.dll'
assert sha(component)=='c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838'
assert sha(runtime)=='ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82'
for p,folder in [(component,'component'),(runtime,'stock-runtime')]:
    (out/folder).mkdir();shutil.copy2(p,out/folder/p.name)
config=root/'runtime-baseline/instance/config/super_resolution/config.toml'
original=config.read_bytes();(out/'original-config.txt').write_bytes(original)
text=original.decode('utf-8').replace('\r\n','\n').replace('\r','')
for key,value in [('mode','X2'),('provider','wisteria:dlssg_fg'),('backend','wisteria:dlssg')]:
    text,n=re.subn(r'(\[frame_generation\][\s\S]*?'+key+r' = )"[^"]+"',lambda m:m[1]+'"'+value+'"',text,count=1);assert n==1
d=tomllib.loads(text);assert d['upscale_algo']=='dlss' and not d['debug']['skip_init_vulkan'] and d['presentation']['backend']=='VULKAN'
config.write_bytes(text.encode('utf-8'))
manifest={**sr,'run_id':out.name,'status':'PREPARED_NOT_LAUNCHED','fg':'X2','requested_count':1,'standalone_gate':previous.name,
          'component_sha256':sha(component),'fg_runtime_sha256':sha(runtime),'original_config_sha256':hashlib.sha256(original).hexdigest(),'test_config_sha256':sha(config)}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
(root/'builds/experimental/dlss-sr/current-combined-run.txt').write_text(str(out))
print(out);print('PREPARED DLSS SR + DLSS-G X2 count1; same SR/Wisteria artifacts as standalone')
