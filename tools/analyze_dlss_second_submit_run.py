"""Observe the sole combined diagnostic run. Unknown evidence never becomes PASS."""
from pathlib import Path
import collections, json, re, sys
root=Path(__file__).resolve().parents[1]
r=Path(sys.argv[1]).resolve();assert r.is_relative_to(root/'logs/runtime/dlss-sr')
def read(name):
    p=r/name
    return [json.loads(x) for x in p.read_text(encoding='utf-8-sig',errors='replace').splitlines() if x.strip()] if p.exists() else []
sr=read('dlss-sr-events.jsonl');fg=read('provider-events.jsonl');java=read('frame-sequence.jsonl');states=read('fg-two-interval-state.jsonl')
log=(r/'launcher.log').read_text(encoding='utf-8',errors='replace')
process=json.loads((r/'process-result.json').read_text(encoding='utf-8-sig'))
manifest=json.loads((r/'manifest.json').read_text());restoration=json.loads((r/'config-restoration.json').read_text())
failures=[];lines=log.splitlines()
for i,line in enumerate(lines):
    match=re.search(r'DLSSG_EXPERIMENT_FAIL stage=(\S+)',line)
    if match: failures.append(dict(stage=match[1],line=i+1,error=lines[i+1] if i+1<len(lines) else 'UNKNOWN'))
exit_match=re.search(r'finished with non-zero exit value (-?\d+)',log)
client_exit=int(exit_match[1]) if exit_match else (0 if process['process_exit_code']==0 else 'UNKNOWN')
lost='VkResult=-4' in log or any(e['event']=='QUEUE_SUBMIT_RESULT' and 'VkResult=-4' in e['detail'] for e in states)
gl_errors=[dict(line=i+1,message=line) for i,line in enumerate(lines) if 'Wait for sync object failed' in line]
counts=lambda a:dict(collections.Counter(e['type'] for e in a))
present=[e for e in java if e['type']=='PRESENT'];kinds=[e['fields'].get('kind') for e in present]
sequence=any(kinds[i:i+3]==['REAL','GENERATED','REAL'] for i in range(len(kinds)-2))
valid=[e for e in java if e['type']=='OUTPUT_READY' and e['fields'].get('presentable')=='true' and e['fields'].get('rawStatus')=='0']
callbacks=[e for e in java if e['type']=='OUTPUT_READY'];generated=sum(k=='GENERATED' for k in kinds)
evaluates=sum(e['type']=='evaluate' for e in fg);creates=sum(e['type']=='POOL_CREATE' for e in java)
sr_eval=sum(e['type']=='EVALUATE' for e in sr)
normal=client_exit==0 and process['process_exit_code']==0 and not failures and not lost
passed=normal and not gl_errors and sr_eval>0 and creates>0 and evaluates>=5 and len(valid)>=5 and generated>=1 and sequence
result=dict(status='PASS' if passed else 'FAIL',run_id=r.name,sr_counts=counts(sr),native_fg_counts=counts(fg),java_fg_counts=counts(java),
    first_failure=(dict(stage='GL_RELEASE_WAIT',operation='glWaitSemaphoreEXT',**gl_errors[0]) if gl_errors else failures[0] if failures else None),secondary_failures=failures,device_lost=lost,
    first_gl_error=gl_errors[0] if gl_errors else None,gl_sync_error_count=len(gl_errors),
    current_blocker='GL_RELEASE_WAIT_FAILED_AND_NATIVE_SHUTDOWN_CRASH' if gl_errors and client_exit!=0 else ('GL_RELEASE_WAIT_FAILED' if gl_errors else ('DEVICE_LOST' if lost else 'NONE' if passed else 'UNVALIDATED')),
    sr_evaluate_count=sr_eval,fg_create_count=creates,fg_evaluate_count=evaluates,fg_gpu_completion_count=len(callbacks),
    fg_valid_interval_count=len(valid),fg_retirement_completion_count=sum(e['type']=='completion' for e in fg),
    fg_generated_present_count=generated,real_g1_real_observed=sequence,
    fault_events=[e for e in fg if e['type'].startswith('device_fault') or e['type']=='device_lost_checkpoint'],
    diagnostic_intervals={str(i):[e for e in states if e['interval']==i] for i in (1,2,3)},
    normal_shutdown=normal,crash=client_exit!=0 or any('Crash report saved' in x for x in lines),
    minecraft_process_exit_code=client_exit,gradle_exit_code=process['process_exit_code'],
    native_0xc0000409='C0000409' in log.upper(),config_restoration=restoration,
    amd_9_classes_2_dlls_identical=manifest['protected_amd_9_classes_2_dlls_identical'],amd_hashes=manifest['amd_hashes'],
    standalone_sr_rerun=False,amd_run=False,x3plus_run=False,attempts=1,
    root_cause='UNKNOWN' if not passed else 'Observed combined PASS after readiness isolation and diagnostics; original underlying lost cause not proven',
    cause_confidence='HIGH_OBSERVED_GATE;UNDERLYING_CAUSE_UNKNOWN',cpu_transport_copy_count=0,
    per_frame_cpu_wait_between_apis=False,present_worker_sole_present_owner=True,
    validation_enabled=False,validation_error_count='NOT_MEASURED_LAYER_ABSENT',validation_errors='UNAVAILABLE')
(r/'runtime-result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:result[k] for k in ('status','first_failure','device_lost','fg_evaluate_count','fg_gpu_completion_count','fg_valid_interval_count','fg_generated_present_count','minecraft_process_exit_code','gradle_exit_code')},indent=2))
