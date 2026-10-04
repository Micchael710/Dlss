"""Preserve bounded exported evidence and document only after the sole run has ended."""
from pathlib import Path
import json, collections
root=Path(__file__).resolve().parents[1]
r=Path((root/'builds/experimental/dlss-sr/current-combined-run.txt').read_text()).resolve()
s=json.loads((r/'runtime-result.json').read_text()); assert (r/'process-result.json').exists()
assert s['config_restoration']['status']=='PASS'
log=(r/'launcher.log').read_text(encoding='utf-8',errors='replace').splitlines()
first=s['first_gl_error']['line']-1 if s['first_gl_error'] else 0
(r/'runtime-evidence-extract.log').write_text('\n'.join(log[max(0,first-2):first+39]+['\n--- ACTUAL PROCESS END ---']+log[-18:])+'\n',encoding='utf-8')
samples=[]
for name in ('dlss-sr-events.jsonl','provider-events.jsonl','frame-sequence.jsonl'):
    rows=[json.loads(x) for x in (r/name).read_text().splitlines()]
    if name=='dlss-sr-events.jsonl':
        selected=[e for e in rows if e['type'] in ('VULKAN_INIT_RESULT','NGX_INIT','CAPABILITY','CREATE_FEATURE','CREATE_COMPLETION','GL_VK_INTEROP_READY','SHUTDOWN_BEGIN','NGX_SHUTDOWN','FEATURES_RETIRED','GRAPHICS_BACKEND_DESTROYED')]
    elif name=='frame-sequence.jsonl':
        selected=[e for e in rows if e['type'] in ('POOL_CREATE','OUTPUT_READY','PRESENT','VULKAN_CONSUMER_SUBMIT','BORROWED_QUEUE_JOIN')][:70]
    else:
        selected=rows[:15]+[e for e in rows if e['type'].startswith('device_fault') or e['type']=='device_lost_checkpoint']+rows[-8:]
    samples.extend(dict(file=name,record=e) for e in selected)
(r/'fg-runtime-samples.jsonl').write_text(''.join(json.dumps(e)+'\n' for e in samples),encoding='utf-8')
worker=json.loads((r/'worker-result.json').read_text())
keys=['device_fault_enabled','diagnostic_checkpoints_enabled','device_removed','device_removed_reason','cleanup','create_result',
      'generated_disable_flag_zero_count','d3d12_debug_errors','debug_layer_enabled','if_fail_stage']
(r/'native-diagnostic-summary.json').write_text(json.dumps({k:worker.get(k,'UNKNOWN') for k in keys},indent=2)+'\n')
summary=f"""## 2026-10-04 — second-submit diagnostic, combined outcome FAIL

One combined run: `{r.name}`. No standalone SR rerun, AMD run or x3+ run. Start HEAD `d9648f281d9b9316d9efa615da231b0cd1a864da`.

The first three native input submissions returned VK_SUCCESS and each reached D3D12 completion and slot retirement. The previous second-submit VK_ERROR_DEVICE_LOST did not recur. Observed FG: Create pools {s['fg_create_count']}, Evaluate {s['fg_evaluate_count']}, completion callbacks {s['fg_gpu_completion_count']}, valid intervals {s['fg_valid_interval_count']}, generated presents {s['fg_generated_present_count']}; REAL → G1 → REAL observed {s['real_g1_real_observed']}. These are public worker present events, not a scanout capture.

**Combined FAIL:** the first OpenGL error was `GL_INVALID_OPERATION: Wait for sync object failed` in `glWaitSemaphoreEXT`, from `FrameResourcesSet.awaitCaptureRelease`, launcher line {s['first_gl_error']['line']}. The user closed the game normally, but the process exited {s['minecraft_process_exit_code']} (`0xC0000409`); Gradle exit {s['gradle_exit_code']}. Native bridge recorded DRAINED, device removed false. SR shutdown completion was not observed. No retry. Original device-loss root cause remains **UNKNOWN**; this run changed readiness isolation and source barrier scopes together, so it cannot attribute the previous fault to one change.

Implementation: persistent borrowed readiness pair per capture slot, one signal/consume cycle retired after the existing output fence; no new per-frame host waits or CPU frame transport. Reset/retirement guards remain. JNI ABI4 adds device-fault/checkpoint configuration and first-three-interval state. Diagnostics are opt-in. Optional device_fault feature is queried and enabled correctly; vendor binary is disabled. Both fault and checkpoint extensions were enabled, but neither fault data nor queue checkpoint snapshots were queried because VK_ERROR_DEVICE_LOST did not occur. Checkpoint markers remain alive until slot retirement.

Preflight: actual NVIDIA Vulkan format/usage/extent/sample queries PASS for R32F and RG16F → RG32F floating-point color NEAREST blit. COLOR aspect, mip0, layer0/count1, samples1, 495x278; linear filtering is unused. No compute conversion or CPU conversion. Vulkan validation layer is absent; zero captured VUIDs is **not** a validation PASS. CPU tests rejected 13 stale/readiness cases, 8 binary/pending reuse cases and 4 native slot/command cases. Actual JNI load ABI4, C++JNI, SR NeoForge and Wisteria builds passed. AMD 9 classes + 2 DLLs remain byte-identical by hashes; historical MFG x3 evidence retained.

NGX SDK section 3.4 guarantees input resources return to read state. Caller declares SHADER_READ_ONLY_OPTIMAL; borrowed source barriers remain 5 → 6 → 5, now with explicit compute sampled-read ↔ blit transfer-read scopes, using enabled sync2 in the diagnostic route. This is the documented/caller contract, not a measured image-layout query. [Pinned NVIDIA SDK guide](https://raw.githubusercontent.com/NVIDIA/DLSS/374959484e79a640feaba44c93ac8cfb0a03f5b5/doc/DLSS_Programming_Guide_Release.pdf), [Vulkan blit contract](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html).

Config restored byte-for-byte: `{s['config_restoration']['restored_sha256']}`. Baseline/live launcher unaffected. Exported evidence: `logs/runtime/dlss-sr/{r.name}/fg-two-interval-state.jsonl`, prior two-interval comparison, format preflight, build logs, bounded runtime samples, first GL error stack, shutdown result and runtime-result.json. Full existing raw telemetry and binary samples remain local; no binary upload. Presentation owner remains PresentWorker.

Current blocker: **GL_RELEASE_WAIT_FAILED_AND_NATIVE_SHUTDOWN_CRASH**. Do not call this validated SR+x2. Native crash cause and original lost cause UNKNOWN.

"""
for name in ('CURRENT_STATE.md','DEVELOPMENT_LOG.md','DLSS_SR_DLSSG_X2_INTEROP.md'):
    path=root/'docs'/name;old=path.read_text(encoding='utf-8');split=old.find('\n')
    path.write_text(old[:split+1]+'\n'+summary+old[split+1:],encoding='utf-8')
print(json.dumps({k:s[k] for k in ('sr_counts','fg_create_count','fg_evaluate_count','fg_gpu_completion_count','fg_generated_present_count','current_blocker')},indent=2))
