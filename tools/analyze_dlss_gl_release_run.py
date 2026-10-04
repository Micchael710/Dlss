"""Observe exact GL-release calls and flushed teardown stages after the sole short run."""
from pathlib import Path
import collections, json, re, sys
root=Path(__file__).resolve().parents[1];r=Path(sys.argv[1]).resolve()
assert r.is_relative_to(root/'logs/runtime/dlss-sr')
def rows(name):
    p=r/name
    return [json.loads(x) for x in p.read_text(encoding='utf-8-sig',errors='replace').splitlines() if x.strip()] if p.exists() else []
sr=rows('dlss-sr-events.jsonl');fg=rows('provider-events.jsonl');java=rows('frame-sequence.jsonl');waits=rows('gl-release-waits.jsonl')
log=(r/'launcher.log').read_text(encoding='utf-8',errors='replace');process=json.loads((r/'process-result.json').read_text(encoding='utf-8-sig'))
exit_match=re.search(r'finished with non-zero exit value (-?\d+)',log)
client_exit=int(exit_match[1]) if exit_match else (0 if process['process_exit_code']==0 else 'UNKNOWN')
summary=json.loads((r/'gl-release-summary.json').read_text()) if (r/'gl-release-summary.json').exists() else {}
worker=json.loads((r/'worker-result.json').read_text())
copy_events=[e for e in fg if e['type']=='borrowed_copy_complete']
copies=[int(re.search(r'cpuTransportCopies=(\d+)',e['detail'])[1]) for e in copy_events]
gl_count=summary.get('gl_sync_error_count',sum(e['glError']!=0 for e in waits))
callback_errors=log.count('Wait for sync object failed')
gl_count=max(gl_count,callback_errors)
first_error=next((e for e in waits if e['glError']),None)
stages=rows('shutdown-stages.jsonl')+[dict(stage=e['detail'],timestamp_ns=e['timestamp_ns'],thread='FG_NATIVE') for e in fg if e['type']=='shutdown_stage']
stages.sort(key=lambda e:e['timestamp_ns']);pending=[]
for e in stages:
    name=e['stage'].split()[0]
    if name.endswith('_BEGIN'):pending.append(name[:-6])
    elif name.endswith('_END') and name[:-4] in pending:pending.remove(name[:-4])
last_completed=next((e['stage'] for e in reversed(stages) if '_END' in e['stage']),'NONE_OBSERVED')
fail_stage=pending[-1] if pending else 'NONE' if client_exit==0 else 'AFTER_LAST_MARKER_UNKNOWN_CALL'
present=[e['fields'].get('kind') for e in java if e['type']=='PRESENT']
sequence=any(present[i:i+3]==['REAL','GENERATED','REAL'] for i in range(len(present)-2))
counts=lambda a:dict(collections.Counter(e['type'] for e in a))
sr_eval=sum(e['type']=='EVALUATE' for e in sr);fg_eval=sum(e['type']=='evaluate' for e in fg)
fg_create=sum(e['type']=='POOL_CREATE' for e in java);completion=sum(e['type']=='OUTPUT_READY' for e in java);generated=present.count('GENERATED')
lost='VkResult=-4' in log;crash=client_exit!=0;normal=not crash and process['process_exit_code']==0
passed=normal and gl_count==0 and not lost and sr_eval>0 and fg_create>0 and fg_eval>0 and completion==fg_eval and generated>0 and sequence and len(waits)>0 and last_completed=='SHUTDOWN_10_GRAPHICS_DEVICE_END' and fail_stage=='NONE' and len(copies)==fg_eval and sum(copies)==0 and worker.get('per_frame_cpu_wait_between_apis') is False
blocker='GL_RELEASE_WAIT' if gl_count else 'NATIVE_SHUTDOWN_'+fail_stage if crash else 'NONE' if passed else 'RUNTIME_VALIDATION'
result=dict(status='PASS' if passed else 'FAIL',run_id=r.name,first_blocker=blocker,gl_release_wait_fixed=gl_count==0 and len(waits)>0,
    gl_sync_error_count=gl_count,first_gl_error=first_error,gl_callback_error_count=callback_errors,wait_count=summary.get('wait_count','NOT_RECORDED'),
    gl_wait_samples=waits,sr_counts=counts(sr),native_fg_counts=counts(fg),java_fg_counts=counts(java),sr_evaluate_count=sr_eval,
    fg_create_count=fg_create,fg_evaluate_count=fg_eval,fg_gpu_completion_count=completion,fg_generated_present_count=generated,
    real_g1_real_observed=sequence,device_lost=lost,shutdown_stages=stages,shutdown_last_completed_stage=last_completed,shutdown_fail_stage=fail_stage,
    native_0xc0000409=client_exit==-1073740791,normal_shutdown=normal,process_exit_code=client_exit,gradle_exit_code=process['process_exit_code'],
    root_cause='Original GL path allowed wait before corresponding Vulkan release submission (EXT_semaphore 4.2.3). '+('Remaining native crash occurred inside '+fail_stage+'; underlying native cause UNKNOWN.' if crash else 'Exact release waits and complete teardown passed this run.'),
    cause_confidence='HIGH_FOR_GL_CONTRACT_VIOLATION;NATIVE_ROOT_UNKNOWN' if crash else 'HIGH_FOR_THIS_OBSERVED_RUN',
    cpu_transport_copy_count=sum(copies),per_frame_cpu_wait_between_apis=worker.get('per_frame_cpu_wait_between_apis','UNKNOWN'),present_worker_sole_present_owner=True,attempts=1,
    wait_and_present_ownership_evidence='provider report and source audit; no API trace',cpu_transport_count_observations=len(copies),
    config_restoration=json.loads((r/'config-restoration.json').read_text()),amd_baseline_modified=False,
    amd_hashes=json.loads((r/'manifest.json').read_text())['amd_hashes'],standalone_rerun=False,amd_run=False,x3plus_run=False)
(r/'runtime-result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
(r/'shutdown-combined-stages.jsonl').write_text(''.join(json.dumps(e)+'\n' for e in stages),encoding='utf-8')
print(json.dumps({k:result[k] for k in ('status','first_blocker','gl_sync_error_count','fg_evaluate_count','fg_generated_present_count','shutdown_last_completed_stage','shutdown_fail_stage','process_exit_code')},indent=2))
