"""Extract observed combined-run evidence. Does not manufacture a GPU PASS."""
from pathlib import Path
import collections, json, re, sys
root=Path(__file__).resolve().parents[1]
r=Path(sys.argv[1]).resolve();assert r.is_relative_to(root/'logs/runtime/dlss-sr')
read=lambda name:[json.loads(x) for x in (r/name).read_text().splitlines()]
sr=read('dlss-sr-events.jsonl');fg=read('provider-events.jsonl');java=read('frame-sequence.jsonl')
counts=lambda a:dict(collections.Counter(e['type'] for e in a))
log=(r/'launcher.log').read_text(errors='replace');lines=log.splitlines()
failures=[]
for i,line in enumerate(lines):
    match=re.search(r'DLSSG_EXPERIMENT_FAIL stage=(\S+)',line)
    if match:failures.append({'stage':match[1],'error':lines[i+1] if i+1<len(lines) else 'UNKNOWN','line':i+1})
exit_match=re.search(r'finished with non-zero exit value (-?\d+)',log)
gradle=json.loads((r/'process-result.json').read_text(encoding='utf-8-sig'))
worker=json.loads((r/'worker-result.json').read_text())
restoration=json.loads((r/'config-restoration.json').read_text())
manifest=json.loads((r/'manifest.json').read_text())
summary=dict(run_id=r.name,status='FAIL' if failures or exit_match or gradle['process_exit_code'] else 'UNVALIDATED',
    sr_counts=counts(sr),native_fg_counts=counts(fg),java_fg_counts=counts(java),
    first_failure=failures[0] if failures else None,secondary_failures=failures[1:],
    integration_failure_file_is_secondary=(r/'integration-failure.txt').read_text(),
    fg_create_count=sum(e['type']=='POOL_CREATE' for e in java),native_create_result=worker.get('create_result'),
    fg_evaluate_count=sum(e['type']=='evaluate' for e in fg),
    fg_completion_callback_count=sum(e['type']=='OUTPUT_READY' for e in java),
    fg_retirement_completion_count=sum(e['type']=='completion' for e in fg),
    fg_generated_present_count=sum(e['type']=='PRESENT' and e['fields'].get('kind')=='GENERATED' for e in java),
    fg_real_present_count=sum(e['type']=='PRESENT' and e['fields'].get('kind')=='REAL' for e in java),
    input_copy_records=[e for e in fg if e['type']=='borrowed_copy_recorded'],
    input_copy_submissions=[e for e in fg if e['type']=='borrowed_copy_submit'],
    input_copy_retirement_completions=[e for e in fg if e['type']=='borrowed_copy_complete'],
    completion_snapshots=[e for e in java if e['type']=='OUTPUT_READY'],
    readiness_receipts=[e for e in java if e['type']=='BORROWED_QUEUE_JOIN'],
    sr_pre_world_gates=[e for e in sr if e['type'] in ('VULKAN_INIT_RESULT','NGX_INIT','CAPABILITY','CREATE_FEATURE','GL_VK_INTEROP_READY')],
    pre_world_queue_join='CPU_CONTRACT_CONFIGURED;GPU_JOIN_NOT_YET_OBSERVED_BEFORE_WORLD',
    matrix_evidence=[x for x in lines if 'DLSSG_MATRIX_EVIDENCE' in x],
    matrix_warning=bool('cameraViewToClip is not invertible' in log),matrix_primary_fail_stage=False,
    source_layout_evidence='GL external signal declares SHADER_READ_ONLY; native copies record 5->6->5; post-NGX layout correctness NOT GPU-validated',
    device_lost='input queue submit VkResult=-4' in log,
    root_cause='Second FG Vulkan input queue submission returned VK_ERROR_DEVICE_LOST after first interval reached D3D12 completion; underlying GPU fault/ordering/layout cause UNPROVEN',
    cause_confidence='HIGH_FOR_OBSERVED_GATE;UNDERLYING_GPU_CAUSE_UNKNOWN',
    normal_shutdown=False if exit_match else None,managed_java_crash=(r/'minecraft-crash.txt').exists(),
    minecraft_process_exit_code=int(exit_match[1]) if exit_match else 0,gradle_exit_code=gradle['process_exit_code'],
    historical_native_crash_0xc0000409_reproduced='C0000409' in log.upper(),
    sr_shutdown_events=[e for e in sr if e['type'] in ('SHUTDOWN_BEGIN','NGX_SHUTDOWN','FEATURES_RETIRED','GRAPHICS_BACKEND_DESTROYED')],
    config_restoration=restoration,amd_hashes=manifest['amd_hashes'],amd_9_classes_2_dlls_identical=manifest['protected_amd_9_classes_2_dlls_identical'],
    rerun=False,standalone_rerun=False,requested_count=manifest['requested_count'],
    visual='NOT_VALIDATED_BECAUSE_DEVICE_LOST_EARLY;USER_CONFIRMED_WORLD_ENTRY_ONLY',
    gpu_fixture='NOT_RUN;existing harness does not cover the GL/SR borrowed producer; no new GPU investigation',
    cpu_transport_copy_count=0,per_frame_cpu_wait_in_new_queue_join=False,
    existing_vulkan_reuse_waits='Command/capture rings retain their existing bounded Vulkan fence backpressure; no new cross-API host wait')
(r/'runtime-result.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:summary[k] for k in ['status','first_failure','fg_create_count','fg_evaluate_count','fg_completion_callback_count','fg_generated_present_count','device_lost','minecraft_process_exit_code','gradle_exit_code']},indent=2))
