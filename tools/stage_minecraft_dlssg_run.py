"""Prepare exactly one isolated run; no process is started by this tool."""
from pathlib import Path
import hashlib,json,shutil,sys,datetime,re,subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
mode=sys.argv[1] if len(sys.argv)>1 else 'shadow'
assert mode in ('shadow','presentation')
artifact=json.loads((ROOT/'builds/experimental/dlssg-x2/artifact-manifest.json').read_text())
if mode=='presentation':
    previous=Path(sys.argv[2]).resolve()
    assert previous.is_relative_to(ROOT/'logs/runtime/minecraft-dlssg-x2')
    gate=json.loads((previous/'result.json').read_text())
    assert gate.get('MINECRAFT_DLSSG_SHADOW_X2')=='PASS','Shadow evidence required'
    assert gate.get('artifacts')==artifact['artifacts'],'Shadow must test these same candidates'
runid=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')[:-3]
out=ROOT/'logs/runtime/minecraft-dlssg-x2'/runid
out.mkdir(parents=True)
component=ROOT/'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
runtime=ROOT/'experiments/ngx-dlssg-probe/vendor/nvngx_dlssg.dll'
assert sha(component)=='c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838'
assert sha(runtime)=='ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82'
for p,folder in [(component,'component'),(runtime,'stock-runtime')]:
    (out/folder).mkdir();shutil.copy2(p,out/folder/p.name)
config=ROOT/'runtime-baseline/instance/config/super_resolution/config.toml'
old=config.read_bytes();(out/'original-test-config.toml').write_bytes(old)
text=old.decode('utf-8')
assert 'upscale_ratio = 1.7' in text and 'upscale_algo = "fsr1"' in text
assert '[optiscaler]' in text and 'dll_path = ""' in text and 'enabled = false' in text
assert 'backend = "VULKAN"' in text and 'mode = "X2"' in text
text,n=re.subn(r'(\[frame_generation\][\s\S]*?provider = )"[^"]+"',r'\1"wisteria:dlssg_fg"',text,count=1);assert n==1
text,n=re.subn(r'(\[frame_generation\][\s\S]*?backend = )"[^"]+"',r'\1"wisteria:dlssg"',text,count=1);assert n==1
config.write_bytes(text.encode('utf-8'))
manifest={'run_id':runid,'mode':mode,'attempt_limit':1,'status':'PREPARED_NOT_LAUNCHED',
 'project_base':'289862295b04422a41dc0a5a47068ab1b00c9bd4',
 'implementation_commit':subprocess.check_output(['C:/Users/micha/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
 'artifacts':artifact['artifacts'],
 'original_test_config_sha256':hashlib.sha256(old).hexdigest(),'test_config_sha256':sha(config),
 'component_sha256':sha(component),'runtime_sha256':sha(runtime),
 'profile':'Minecraft1.21.1 / NeoForge21.1.219 / Java21 / Complementary / SR1.7 / OpenGLFSR1 / VulkanPresent / OptiScalerOFF',
 'presentation_generated_enabled':mode=='presentation','cpu_frame_transport':False,
 'runtime_path':'runtime-baseline/instance','gpu_uuid':'01895b66d1ca454d88788dd21fdef638','gpu_luid':'4c29010000000000','driver':'596.49'}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(ROOT/'builds/experimental/dlssg-x2/current-run.txt').write_text(str(out),encoding='utf-8')
print(json.dumps(manifest,indent=2));print(out)
