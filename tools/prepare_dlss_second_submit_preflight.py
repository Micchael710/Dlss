"""Bounded prior-interval comparison and candidate preflight; never launches a game."""
from pathlib import Path
import json, re, shutil, hashlib, zipfile
root=Path(__file__).resolve().parents[1]
out=Path((root/'builds/experimental/dlss-sr/current-combined-run.txt').read_text()).resolve()
prior=root/'logs/runtime/dlss-sr/20261004-181754-546-handoff-x2'
records=[]
for name in ('provider-events.jsonl','frame-sequence.jsonl','dlss-sr-events.jsonl'):
    for line in (prior/name).read_text().splitlines():
        e=json.loads(line)
        detail=str(e.get('detail',''))+' '+str(e.get('message',''))
        fields=e.get('fields',{})
        match=re.search(r'(?:realFrameId|PAIR_ID)=(\d+)',detail)
        frame=int(match[1]) if match else int(fields.get('realFrameId',fields.get('realId',0)) or 0)
        if frame in (1,2): records.append(dict(file=name,interval=frame,record=e))
(out/'prior-two-interval-comparison.json').write_text(json.dumps(dict(prior_run=prior.name,records=records,
    unrecorded=['second native input command handle','native command generations','native command pool',
                'actual binary consumption completion','release semaphore identities'],
    conclusion='Distinct capture generations, SR commands/fences and FG destination slots. Shared source images/readiness pair. Underlying lost cause UNKNOWN.'),indent=2)+'\n')
base=root/'builds/experimental/dlss-sr/second-submit'
for name in ('format-preflight.json','format-query-events.jsonl','vulkan-validation.log','native-build.log','sr-build.log','wisteria-build.log','preflight-build.log','reuse-build.log'):
    shutil.copy2(base/name,out/name)
formats=json.loads((out/'format-preflight.json').read_text());assert formats['format_gate']=='PASS'
for name in ('sr-build.log','wisteria-build.log'): assert 'BUILD SUCCESSFUL' in (out/name).read_text(encoding='utf-8',errors='replace')
assert 'Linking CXX shared library' in (out/'native-build.log').read_text(encoding='utf-8',errors='replace')
manifest=json.loads((out/'manifest.json').read_text());assert manifest['bridge_abi']==4
assert manifest['protected_amd_9_classes_2_dlls_identical']
with zipfile.ZipFile(root/manifest['wisteria_jar']) as jar:
    assert hashlib.sha256(jar.read('natives/windows-x64/wisteria_dlssg_bridge.dll')).hexdigest()==manifest['bridge_sha256']
preflight=dict(status='PASS_CONTRACTS_AND_FORMAT_QUERIES',gpu_execution_validated=False,root_cause='UNKNOWN',
    tests={'borrowed_readiness_rejections':13,'binary_and_pending_reuse_rejections':8,'native_slot_retirement_rejections':4,'actual_jni_abi':4},
    tests_provenance='CPU test executions captured by Codex; booleans are guard inputs, not simulated GPU completion',
    binary_semaphore_contract='persistent per-capture-slot pair; signal once, consume submitted once, reuse after tracked output fence',
    fence_contract='existing Vulkan fence status checked before reset; no additional host wait',
    command_contract='SR reset guarded by fence; native input reset only after lease retired and D3D12 done',
    slot_contract='lease release requires D3D12 done, flags callback, caller Vulkan output/presentation retirement',
    post_ngx_layout_contract='SDK section 3.4 restores inputs to read state; caller declares SHADER_READ_ONLY_OPTIMAL; runtime validation not yet observed',
    sdk_guide='https://raw.githubusercontent.com/NVIDIA/DLSS/374959484e79a640feaba44c93ac8cfb0a03f5b5/doc/DLSS_Programming_Guide_Release.pdf',
    motion_conversion='RG16F -> RG32F floating-point color NEAREST blit; support flags and exact usage/extent/sample query PASS',
    depth_conversion='R32F -> R32F color NEAREST blit; support flags and exact usage/extent/sample query PASS',
    source_usage='SAMPLED|STORAGE|TRANSFER_SRC|TRANSFER_DST',destination_usage=15,
    subresource='COLOR,mip=0,layer=0,count=1,samples=1,495x278',
    validation_enabled=False,validation_reason='VK_LAYER_KHRONOS_validation absent; zero VUIDs is not validation PASS',
    first_failure_action='STOP, capture optional fault/checkpoints, retain unsafe resources, no second launch',
    standalone_sr_rerun=False,amd_rerun=False,mfg_3plus_rerun=False,attempt_limit=1)
(out/'second-submit-preflight.json').write_text(json.dumps(preflight,indent=2)+'\n')
print(out); print('PASS builds + CPU contracts + actual JNI ABI4 + NVIDIA format queries + AMD hashes; GPU outcome UNKNOWN')
