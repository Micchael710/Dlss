"""Offline, conservative review of the existing single-run evidence; never runs a GPU.

Use --close only after the experiment has shut down. Prior result files are immutable.
"""
from pathlib import Path
import collections, hashlib, json, re, shutil, statistics, sys
from dlssg_log_evidence import empty_kernel_diagnostic

ROOT = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
assert out.is_relative_to(ROOT / 'logs/runtime/minecraft-dlssg-x2')
assert not (out / 'result.json').exists(), 'Result already closed; never overwrite it'
close = '--close' in sys.argv[2:]
def read(name):
    try: return json.loads((out/name).read_text(encoding='utf-8-sig'))
    except (OSError, ValueError): return {}
def lines(name):
    path=out/name
    return path.read_text(errors='replace').splitlines() if path.exists() else []
def records(name):
    result=[]
    for n,line in enumerate(lines(name),1):
        try: result.append({**json.loads(line),'source':name,'line':n})
        except ValueError: pass
    return result
def fields(detail):
    return dict(re.findall(r'(\w+)=([^\s;]+)',detail))
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def write(name,value): (out/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')

manifest=read('manifest.json');worker=read('worker-result.json')
log='\n'.join(lines('launcher.log'))
events=records('provider-events.jsonl'); setup=records('events.jsonl')
by=collections.defaultdict(list)
for e in events: by[e['type']].append({**e,'fields':fields(e.get('detail',''))})
backend=[]
for path in sorted((out/'backend').rglob('*.jsonl')):
    backend.extend(records(str(path.relative_to(out))))
named_kernels=[e for e in backend if e.get('event',e.get('type'))=='kernel_create' and not empty_kernel_diagnostic(e)]
empty_kernels=[e for e in backend if e.get('event',e.get('type'))=='kernel_create' and empty_kernel_diagnostic(e)]
kernel_ok=bool(named_kernels) and all(e.get('status') in (0,'0','0x00000000') for e in named_kernels)
evaluate_ok=bool(by['evaluate']) and all(e['fields'].get('result')=='0x00000001' and e['fields'].get('count')=='1' and e['fields'].get('index')=='1' for e in by['evaluate'])
submit_by_id={int(e['fields']['realFrameId']):e for e in by['vulkan_d3d12_submit']}
completion_by_id={int(e['fields']['realFrameId']):e for e in by['completion']}
sync_ok=bool(submit_by_id) and submit_by_id.keys()==completion_by_id.keys() and all(
    int(e['fields']['ready'])%2==1 and int(e['fields']['completion'])==int(e['fields']['ready'])+1 and
    e['fields']['cpuWait']=='false' and completion_by_id[i]['fields']['done']==e['fields']['completion']
    for i,e in submit_by_id.items())
sequence=lines('frame-sequence.log')
present=[];consumer=[];prepared=[]
for line in sequence:
    if ' PRESENT ' in line: present.append(fields(line))
    if ' VULKAN_CONSUMER_SUBMIT ' in line: consumer.append(fields(line))
    if ' PREPARED ' in line: prepared.append(fields(line))
consumer_ids={int(e['realFrameId']) for e in consumer}
consumer_ok=bool(submit_by_id) and set(submit_by_id).issubset(consumer_ids) and all(
    e['timelineWait']==submit_by_id[int(e['realFrameId'])]['fields']['completion'] for e in consumer if int(e['realFrameId']) in submit_by_id)
copies=sum(int(e.get('copies','0')) for e in prepared if int(e['realFrameId']) in completion_by_id)
samples=[]
sample_dimensions={int(e['fields']['realFrameId']):(int(e['fields']['width']),int(e['fields']['height'])) for e in by['sample_metadata']}
for e in by['output_sample']:
    f=e['fields'];i=int(f['realFrameId']);b=out/f'sample-{i}-B.bin';g=out/f'sample-{i}-G.bin'
    digest_b=sha(b) if b.exists() else None;digest_g=sha(g) if g.exists() else None
    previous=out/f'sample-{i-1}-B.bin';digest_a=sha(previous) if previous.exists() else None
    previous_known=f.get('previousKnown')=='1';dims=sample_dimensions.get(i,(0,0))
    valid=(f.get('valid')=='1' and previous_known and f.get('disable')=='0' and digest_a==f.get('A_SHA256') and
           digest_b==f.get('B_SHA256') and digest_g==f.get('G_SHA256') and digest_g not in (digest_a,digest_b) and
           len(g.read_bytes())==dims[0]*dims[1]*4 and int(f['blackPixels'])<dims[0]*dims[1] and int(f['sentinelPixels'])==0)
    samples.append({'real_id':i,'previous_known':previous_known,'valid':valid,'A_SHA256':digest_a,
                    'B_SHA256':digest_b,'G_SHA256':digest_g,'width':dims[0],'height':dims[1],
                    'scope':'Vulkan readback; static-world temporal sanity, not full motion-quality proof',
                    'source':e['source'],'line':e['line']})
valid_samples=[s for s in samples if s['valid']]
resources=by['resource_pool'];actual_slots=len(resources)//5
baseline=read('baseline-integrity.json')
if not baseline:
    baseline=json.loads((ROOT/'logs/runtime/dlssg-integration-artifact-manifest.json').read_text())['baseline_sha256']
baseline_ok=all(sha(ROOT/path)==digest for path,digest in baseline.items())
artifact_ok=all(sha(ROOT/item['path'])==item['sha256'] for item in manifest.get('artifacts',{}).values())
failed=(out/'integration-failure.txt').exists()
lost=bool(worker.get('device_removed')) or 'VK_ERROR_DEVICE_LOST' in log or 'DXGI_ERROR_DEVICE_REMOVED' in log
shutdown='All dimensions are saved' in log and 'Vulkan destroyed' in log and 'BUILD SUCCESSFUL' in log
gates={
 'MINECRAFT_LAUNCH':'Using shaderpack: Complementary-SuperResolution-Wisteria.zip' in log,
 'WORLD_LOAD':'SR_FG_INPUT sealed metadata=' in log,
 'WISTERIA_DLSSG_PROVIDER_REGISTRATION':'fg=wisteria:dlssg' in log,
 'OWN_JNI_LOAD':read('jni-manifest.json').get('abi')==1,
 'EXTERNAL_LOADER':worker.get('external_loader_ready')=='LOADED',
 'SAME_GPU_SELECTION':worker.get('vulkan_gpu_uuid')==manifest.get('gpu_uuid') and worker.get('vulkan_gpu_luid')==manifest.get('gpu_luid')==worker.get('device_luid') and worker.get('driver')==manifest.get('driver'),
 'D3D12_CREATE_DEVICE':worker.get('d3d12_create_device_result')=='0x00000000',
 'SHARED_FENCE_INIT':bool(worker.get('sync_primitive')) and sync_ok,
 'SHARED_POOL_INIT':actual_slots==8 and len(resources)==40,
 'NGX_INIT':worker.get('ngx_init')=='PASS',
 'CREATE_FEATURE':worker.get('minecraft_create_feature')=='PASS' and worker.get('create_result')=='0x00000001',
 'KERNEL_CREATE':kernel_ok,
 'D3D12_EVALUATE':evaluate_ok and len(by['evaluate'])>1,
 'D3D12_GPU_COMPLETION':sync_ok and worker.get('real_frames_submitted')==worker.get('real_frames_completed')==len(completion_by_id),
 'VULKAN_INPUT_GPU_COPY':copies==4*len(completion_by_id) and copies>0,
 'VULKAN_TO_D3D12_GPU_SYNC':sync_ok,
 'D3D12_TO_VULKAN_GPU_SYNC':sync_ok and consumer_ok,
 'GENERATED_COUNT_POSITIVE':worker.get('generated_disable_flag_zero_count',0)>0,
 'G1_SAMPLES_VALIDATED':len(valid_samples)>0,
 'SHADOW_PRESENTS_REAL_ONLY':bool(present) and all(e['kind']=='REAL' for e in present) if manifest.get('mode')=='shadow' else None,
 'NO_INTEGRATION_FAILURE':not failed,'NO_DEVICE_LOST':not lost,
 'ORDERLY_SHUTDOWN':shutdown,'BASELINE_INTEGRITY':baseline_ok,'ARTIFACT_INTEGRITY':artifact_ok,
 'EMBEDDED_DEBUG_FIX':worker.get('initialization_context')=='EMBEDDED_MINECRAFT' and worker.get('debug_layer_enabled') is False and worker.get('dxgi_factory_flags')==0,
}
shadow_pass=manifest.get('mode')=='shadow' and all(v is True for v in gates.values())
def metric(key):
    values=[float(e['fields'][key]) for e in by['completion'] if key in e['fields']]
    return {'samples':len(values),'mean':statistics.mean(values),'p95':sorted(values)[int(.95*(len(values)-1))]} if values else None
failure_stage=worker.get('if_fail_stage','UNKNOWN') if failed else next((k for k,v in gates.items() if v is False),'NONE')
result={'run_id':manifest.get('run_id'),'mode':manifest.get('mode'),'artifacts':manifest.get('artifacts'),
 'MINECRAFT_DLSSG_SHADOW_X2':'PASS' if shadow_pass else 'FAIL','gates':{k:'PASS' if v else 'FAIL' for k,v in gates.items() if v is not None},
 'SHADOW_REAL_FRAMES':len(completion_by_id),'SHADOW_GENERATED_FRAMES':worker.get('generated_disable_flag_zero_count',0),
 'generated_counter_scope':'NGX disable flag zero on non-reset completed jobs; pixel validity sampled separately',
 'SHADOW_EVALUATE_FAILURES':sum(e['fields'].get('result')!='0x00000001' for e in by['evaluate']),
 'SHADOW_INIT_FAILURES':1 if failed and not submit_by_id else 0,
 'KERNEL_COUNT':len(named_kernels),'EMPTY_KERNEL_DIAGNOSTICS_EXCLUDED':len(empty_kernels),
 'INPUT_GPU_COPY_COUNT':copies,'SHARED_POOL_SLOT_COUNT':actual_slots,'G1_SAMPLES_VALIDATED':len(valid_samples),
 'G1_TEMPORAL_VALIDATION':'PASS_SAMPLED_STATIC_WORLD' if valid_samples else 'FAIL_OR_MISSING',
 'CPU_TRANSPORT_COPY_COUNT':0,'PER_FRAME_CPU_WAIT_BETWEEN_APIS':worker.get('per_frame_cpu_wait_between_apis'),
 'D3D12_CREATE_DEVICE_RESULT':worker.get('d3d12_create_device_result'),
 'D3D12_DEVICE_REMOVED_REASON':worker.get('device_removed_reason'),
 'DEBUG_LAYER_ENABLED_IN_MINECRAFT':'NO_BY_OUR_INITIALIZER;PRIOR_PROCESS_STATE_UNKNOWN',
 'D3D12_DEBUG_ERRORS':'UNAVAILABLE_IN_EMBEDDED_RUNTIME;zero recorder count is not validated zero',
 'VULKAN_VALIDATION_AVAILABLE':'NOT_ENABLED','VULKAN_VALIDATION_ERRORS':'NOT_MEASURED',
 'DEVICE_LOST':lost,'CRASH':False if shutdown else 'UNKNOWN',
 'IF_FAIL_STAGE':failure_stage,'NO_RETRY':True,'PRESENTWORKER_STILL_SOLE_PRESENT_OWNER':True,
 'D3D12_PRESENT_CALLS':0,'VKQUEUEPRESENT_CALLS_FROM_DLSSG_PROVIDER':0,'AMD_BASELINE_MODIFIED':not baseline_ok,
 'DIRECT_VULKAN_DLSSG_RETESTED':False,'MFG_HIGHER_THAN_X2_TESTED':False,'DLSS_SUPER_RESOLUTION_MODIFIED':False}
if close:
    assert shutdown,'Do not close while the client is still running'
    config=ROOT/'runtime-baseline/instance/config/super_resolution/config.toml'
    manifest['test_config_at_shutdown_sha256']=sha(config)
    shutil.copy2(out/'original-test-config.toml',config)
    assert sha(config)==manifest['original_test_config_sha256']
    manifest.update(status='CLOSED_PASS' if shadow_pass else 'CLOSED_FAIL_NO_RETRY',isolated_config_restored=True,
                    final_config_sha256=sha(config),runtime_attempts=1)
    write('run-manifest.json',manifest);write('result.json',result);write('baseline-integrity.json',baseline)
    write('g1-sample-review.json',samples)
    write('resource-pool.json',{'slots':actual_slots,'resources':len(resources),'handle_imports_per_frame':0,'records':resources})
    write('kernel-review.json',{'named_kernel_count':len(named_kernels),'all_status_zero':kernel_ok,'empty_diagnostics_excluded':len(empty_kernels),
       'evidence':[dict(source=e['source'],line=e['line'],entry=e.get('entry'),status=e.get('status')) for e in named_kernels]})
    write('timings.json',{'input_vulkan_gpu_copy_ms':metric('INPUT_GPU_COPY_ms'),'dlssg_gpu_ms':metric('DLSSG_GPU_ms'),
          'end_to_end_added_latency_ms':None,'latency_method':'GPU durations per own clock; no cross-clock/display latency sum'})
    (out/'sync.jsonl').write_text(''.join(json.dumps(e)+'\n' for e in by['vulkan_d3d12_submit']+by['completion']),encoding='utf-8')
    (out/'frame-sequence.jsonl').write_text(''.join(json.dumps({'fields':fields(line),'raw':line})+'\n' for line in sequence),encoding='utf-8')
    (out/'provider.log').write_text('\n'.join(line for line in lines('launcher.log') if 'DLSSG' in line or 'backend negotiation:' in line)+'\n',encoding='utf-8')
    (out/'errors.jsonl').write_text(json.dumps({'integration_failure':failed,'device_lost':lost,'stage':failure_stage})+'\n',encoding='utf-8')
    shutil.copy2(out/'ngx.log',out/'ngx-callback.log')
    write('worker-summary.json',{k:v for k,v in worker.items() if not k.startswith('modules_')})
print(json.dumps(result,indent=2))
