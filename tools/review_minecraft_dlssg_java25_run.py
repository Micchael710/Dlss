"""Offline review of the single Java25 run. Capabilities never imply generation PASS."""
from pathlib import Path
import collections, hashlib, json, re, sys, zipfile
from dlssg_log_evidence import empty_kernel_diagnostic

ROOT=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]).resolve()
assert out.is_relative_to(ROOT/'logs/runtime')
assert not (out/'result.json').exists(), 'Never overwrite a closed run'

def read(name):
    p=out/name
    return p.read_text(encoding='utf-8-sig',errors='replace') if p.exists() else ''
def records(name):
    return [{**json.loads(line),'source':name,'line':n} for n,line in enumerate(read(name).splitlines(),1) if line.strip()]
def fields(detail):return dict(re.findall(r'(\w+)=([^\s;]+)',detail))
def write(name,value):(out/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def jsonl(name,values):(out/name).write_text(''.join(json.dumps(v)+'\n' for v in values),encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

manifest=json.loads(read('run-manifest.json'))
jvm=json.loads(read('runtime-jvm.json'))
worker=json.loads(read('worker-result.json'))
assert jvm['java_version']=='25.0.4','Nonconforming Minecraft process'
count=manifest['requested_count'];log=read('launcher.log')
failure=read('integration-failure.txt')
crash='Game crashed! Crash report saved to:' in log
normal_exit='BUILD SUCCESSFUL' in log and 'Vulkan destroyed' in log
assert normal_exit or crash,'Close only after the client exits'
events=records('provider-events.jsonl');seq=records('frame-sequence.jsonl')
by=collections.defaultdict(list)
for e in events:by[e['type']].append({**e,'fields':fields(e.get('detail',''))})
sequences=collections.defaultdict(list)
for e in seq:sequences[e['type']].append(e)
evals={(int(e['fields']['realFrameId']),int(e['fields']['index'])):e for e in by['evaluate']}
completions={(int(e['fields']['realFrameId']),int(e['fields']['index'])):e for e in by['completion']}
ready={(int(e['fields']['realFrameId']),int(e['fields']['index'])):e for e in sequences['OUTPUT_READY']}
valid={key for key,ev in evals.items() if ev['fields']['result']=='0x00000001' and key in completions and key in ready
       and ev['fields']['reset']=='0' and ready[key]['fields']['status']=='ENABLED'
       and completions[key]['fields']['disable']=='0' and ev['fields']['done']==completions[key]['fields']['done']==ready[key]['fields']['completion']
       and int(ev['fields']['VkImage'],16)!=0}
unknown=[e for e in sequences['OUTPUT_READY'] if e['fields']['status']=='UNKNOWN']
present=sequences['PRESENT']
pg=[e for e in present if e['fields']['kind']=='GENERATED'];pr=[e for e in present if e['fields']['kind']=='REAL']
pkeys=[(int(e['fields']['realFrameId']),int(e['fields']['generatedIndex'])) for e in pg]
assert all(key in valid for key in pkeys),'An invalid/UNKNOWN generated output was presented'
assert all(int(e['fields']['VkImage'])==int(evals[key]['fields']['VkImage'],16) for key,e in zip(pkeys,pg)),'Presentation resource mismatch'
assert len(pkeys)==len(set(pkeys)),'Duplicate generated presentation key'
complete_groups={key[0] for key in completions}
resources=by['resource_pool']
baseline={p:{'expected_sha256':h,'actual_sha256':sha(ROOT/p)} for p,h in manifest['baseline_sha256'].items()}
assert all(v['expected_sha256']==v['actual_sha256'] for v in baseline.values())
wbase=ROOT/'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar'
wnow=ROOT/manifest['artifacts']['wisteria']['path']
assert sha(wnow)==manifest['artifacts']['wisteria']['sha256']
with zipfile.ZipFile(wbase) as old,zipfile.ZipFile(wnow) as new:
    names=[n for n in old.namelist() if n.startswith('org/ireallywanttosleep/wisteria/fsr/') and n.endswith('.class')]
    names+=['natives/windows-x64/wisteria_fsr_bridge.dll','natives/windows-x64/amd_fidelityfx_vk.dll']
    amd={n:hashlib.sha256(old.read(n)).hexdigest() for n in names}
    assert len(names)==11 and all(new.read(n)==old.read(n) for n in names)
write('baseline-integrity.json',{'protected_files':baseline,'AMD_9_classes_2_DLLs_identical':amd})
backend=[]
for p in sorted((out/'backend').glob('*.jsonl')):backend+=records(p.relative_to(out).as_posix())
kernels=[e for e in backend if e.get('event')=='kernel_create' and not empty_kernel_diagnostic(e)]
write('worker-summary.json',{k:v for k,v in worker.items() if not k.startswith('modules')})
write('flag-review.json',{'outputs':sequences['OUTPUT_READY'],'UNKNOWN_not_API_disable_value':unknown,
    'completion_scope':'D3D12 same command list containing all Evaluate/status copies; queue Signal after Execute',
    'invalid_output_presentations':0,'root_cause':'SENTINEL_REMAINED_UNWRITTEN_OR_UNCONSUMED; reason UNKNOWN' if unknown else 'NOT_ESTABLISHED'})
write('resource-pool.json',{'observed_shared_images':len(resources),'observed_slots':len(resources)//(4+count),
    'generated_outputs_per_slot':count,'formula':manifest['pool_formula'],'resources':resources,
    'max_in_flight':max((int(e['fields']['inFlight']) for e in sequences['POOL_LEASE']),default=0),
    'exhaustion_events':len(sequences['POOL_EXHAUSTION']),'retirement_events':len(sequences['RETIRED'])})
jsonl('sync.jsonl',by['vulkan_d3d12_submit']+by['completion']+sequences['VULKAN_CONSUMER_SUBMIT'])
jsonl('errors.jsonl',[{'source':'launcher.log','line':n,'raw':line} for n,line in enumerate(log.splitlines(),1)
    if 'DLSSG_EXPERIMENT_FAIL' in line or 'STATUS_NOT_WRITTEN_OR_UNCONSUMED' in line or 'Game crashed!' in line])
(out/'provider.log').write_text('Derived from provider-events.jsonl\n'+'\n'.join(e.get('detail','') for e in events)+'\n',encoding='utf-8')
(out/'ngx-callback.log').write_text(read('ngx.log'),encoding='utf-8')
duration=(int(present[-1]['fields']['timestamp'])-int(present[0]['fields']['timestamp']))/1e9 if len(present)>1 else None
timing={'scope':'provider-owned batches only, startup window; real-only fallback without provider output excluded',
    'duration_seconds':duration,'real_presented':len(pr),'generated_presented':len(pg),'total_presented':len(present),
    'real_presented_fps':len(pr)/duration if duration and duration>0 else 'NOT_MEASURED',
    'generated_presented_fps':len(pg)/duration if duration and duration>0 else 'NOT_MEASURED',
    'presented_fps':len(present)/duration if duration and duration>0 else 'NOT_MEASURED',
    'steady_state_fps':'NOT_MEASURED','late_generated':sum(int(e['fields'].get('lateNs','-1'))>0 for e in pg),
    'late_scope':'accepted request after host pacing deadline, not scanout',
    'valid_unpresented_keys':sorted(valid-set(pkeys)),'duplicate_generated_keys':len(pkeys)-len(set(pkeys)),
    'gpu_ms':[{**e['fields']} for e in by['completion']]}
write('timings.json',timing)
status='FAIL' if unknown or failure or crash else 'UNKNOWN'
result={f'MINECRAFT_DLSSG_X{count+1}':status,'run_id':manifest['run_id'],'Minecraft_process_java':jvm,
    'external_requested_max':worker.get('external_requested_max'),'raw_reported_max':worker.get('raw_reported_multiframecountmax'),
    'capability_provenance':worker.get('capability_provenance'),'capability_hardcoded':False,
    'NGX_INIT':worker.get('ngx_init'),'CREATEFEATURE':worker.get('minecraft_create_feature'),
    'named_kernel_create_events':len(kernels),'kernel_create_success':bool(kernels) and all(e.get('status') in (0,'0','0x00000000') for e in kernels),
    'requested_count':count,'evaluate_success_by_index':{str(i):sum(e['fields']['result']=='0x00000001' for key,e in evals.items() if key[1]==i) for i in range(1,count+1)},
    'completion_by_index':{str(i):sum(key[1]==i for key in completions) for i in range(1,count+1)},
    'status_by_index':{str(i):[{'realFrameId':e['fields']['realFrameId'],'raw_unsigned':e['fields']['rawStatus'],'state':e['fields']['status']} for key,e in ready.items() if key[1]==i] for i in range(1,count+1)},
    'real_completed_groups':len(complete_groups),'flag_eligible_completed_outputs':len(valid),
    'presented_generated':len(pg),'provider_batch_presented_total':len(present),'metrics':timing,
    'generation_count_scope':'completed Evaluate success + written disable0 + non-reset + matching nonzero resource; no pixel proof',
    'bounded_readbacks':len(by['output_sample']),'output_content':'NOT_MEASURED' if not by['output_sample'] else 'REQUIRES_CONTENT_AND_TEMPORAL_REVIEW',
    'temporal_order':'UNKNOWN','x3_complete_present_order':'NOT_OBSERVED' if unknown else 'REQUIRES_REVIEW',
    'invalid_generated_presentations':0,'normal_shutdown':normal_exit,'crash':crash,
    'device_removed':worker.get('device_removed','UNKNOWN'),'device_removed_reason':worker.get('device_removed_reason','UNKNOWN'),
    'if_fail_stage':'OUTPUT_STATUS_NOT_CONFIRMED' if unknown else 'RUNTIME' if failure or crash else 'EVIDENCE_INCOMPLETE',
    'if_fail_index':int(unknown[0]['fields']['index']) if unknown else None,
    'if_fail_status':unknown[0]['fields']['rawStatus'] if unknown else None,
    'ROOT_CAUSE':'SENTINEL_REMAINED_UNWRITTEN_OR_UNCONSUMED; exact cause UNKNOWN' if unknown else 'UNKNOWN',
    'CAUSE_CONFIDENCE':'HIGH_OBSERVATION; UNKNOWN_ROOT_CAUSE','AMD_BASELINE_MODIFIED':False,
    'x4':'NOT_RUN','x5':'NOT_RUN','x6':'NOT_RUN',
    'source_sha256':manifest['source_sha256']}
write('result.json',result)
manifest.update(status='CLOSED_'+status,actual_runtime_java=jvm['java_version'],raw_reported_max=result['raw_reported_max'],
    capability_provenance=result['capability_provenance'])
write('run-manifest.json',manifest)
print(json.dumps({k:v for k,v in result.items() if k not in ('source_sha256','metrics','Minecraft_process_java')},indent=2))
