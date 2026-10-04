"""Prepare one MFG run, never launch a process or manufacture runtime capabilities."""
from pathlib import Path
import datetime, hashlib, json, re, shutil, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
count=int(sys.argv[1]);assert count in (2,3,4)
if count>2:
    previous=Path(sys.argv[2]).resolve()
    assert previous.is_relative_to(ROOT/'logs/runtime')
    gate=json.loads((previous/'result.json').read_text())
    assert gate.get(f'MINECRAFT_DLSSG_X{count}')=='PASS'
runid=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')[:-3]
out=ROOT/f'logs/runtime/minecraft-dlssg-x{count+1}'/runid
out.mkdir(parents=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
artifact=json.loads((ROOT/'builds/experimental/dlssg-x2/artifact-manifest.json').read_text())
for item in artifact['artifacts'].values():assert sha(ROOT/item['path'])==item['sha256']
component=ROOT/'logs/research/dlssg-ampere/deep/components/sdli-0.3.5/version.dll'
runtime=ROOT/'experiments/ngx-dlssg-probe/vendor/nvngx_dlssg.dll'
assert sha(component)=='c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838'
assert sha(runtime)=='ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82'
for p,folder in [(component,'component'),(runtime,'stock-runtime')]:
    (out/folder).mkdir();shutil.copy2(p,out/folder/p.name)
config=ROOT/'runtime-baseline/instance/config/super_resolution/config.toml'
old=config.read_bytes();text=old.decode('utf-8')
assert 'upscale_ratio = 1.7' in text and 'upscale_algo = "fsr1"' in text
assert 'dll_path = ""' in text and 'enabled = false' in text and 'backend = "VULKAN"' in text
(out/'original-test-config.toml').write_bytes(old)
for key,value in [('provider','wisteria:dlssg_fg'),('backend','wisteria:dlssg'),('mode',f'X{count+1}')]:
    text,n=re.subn(r'(\[frame_generation\][\s\S]*?'+key+r' = )"[^"]+"',lambda m:m[1]+'"'+value+'"',text,count=1);assert n==1
config.write_bytes(text.encode('utf-8'))
head=subprocess.check_output(['C:/Users/micha/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
source_paths=list((ROOT/'wisteria/common/src/main/java/org/ireallywanttosleep/wisteria/dlssg').glob('*.java'))+[ROOT/'wisteria/native/dlssg/bridge.cpp',ROOT/'runtime-baseline/build.gradle']
source_sha256={p.relative_to(ROOT).as_posix():sha(p) for p in source_paths}
manifest=dict(run_id=runid,mode='presentation',requested_count=count,requested_indices=list(range(1,count+1)),external_requested_max=count,
             raw_reported_max=None,capability_provenance='pending actual public NGX query after external loader; may be hooked',
             attempt_limit=1,status='PREPARED_NOT_LAUNCHED',implementation_commit=head,artifacts=artifact['artifacts'],baseline_sha256=artifact['baseline_sha256'],
             original_test_config_sha256=hashlib.sha256(old).hexdigest(),test_config_sha256=sha(config),component_sha256=sha(component),runtime_sha256=sha(runtime),
             profile='Minecraft1.21.1 / NeoForge21.1.219 / Java25 / Complementary / FSR1 ratio1.7 / VulkanPresent / OptiScalerOFF',
             required_runtime_java='25.0.4',mod_bytecode_target=21,actual_runtime_java=None,source_sha256=source_sha256,
             pool_formula='floor(6/(requested_count+1))+2; candidate image weight retained even for reset/disabled group',pool_slots=6//(count+1)+2,outputs_per_slot=count,
             samples='at most 3 intervals; start after non-reset camera motion; CPU readback diagnostic only')
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(ROOT/'builds/experimental/dlssg-x2/current-run.txt').write_text(str(out),encoding='utf-8')
print(json.dumps(manifest,indent=2));print(out)
