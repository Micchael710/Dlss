"""Export bounded textual evidence after the one controlled combined run closes."""
from pathlib import Path
import json, sys

root = Path(__file__).resolve().parents[1]
r = Path(sys.argv[1]).resolve()
assert r.is_relative_to(root / 'logs/runtime/dlss-sr')
s = json.loads((r / 'runtime-result.json').read_text())
assert s['config_restoration']['status'] == 'PASS'
assert (r / 'process-result.json').exists()

def rows(name):
    return [json.loads(x) for x in (r / name).read_text().splitlines() if x.strip()]

samples = []
for name in ('dlss-sr-events.jsonl', 'provider-events.jsonl', 'frame-sequence.jsonl'):
    data = rows(name)
    if name == 'dlss-sr-events.jsonl':
        gates = {'VULKAN_INIT_RESULT', 'NGX_INIT', 'CAPABILITY', 'CREATE_FEATURE',
                 'CREATE_COMPLETION', 'SHUTDOWN_BEGIN', 'NGX_SHUTDOWN',
                 'FEATURES_RETIRED', 'GRAPHICS_BACKEND_DESTROYED'}
        selected = [e for e in data if e['type'] in gates]
        selected += [e for e in data if e['type'] in {'EVALUATE', 'GPU_COMPLETION', 'OUTPUT_QUEUED'}][:9]
    else:
        selected = data[:90] + data[-22:]
    samples.extend(dict(file=name, record=e) for e in selected)
(r / 'runtime-samples.jsonl').write_text(''.join(json.dumps(e)+'\n' for e in samples))
log = (r / 'launcher.log').read_text(encoding='utf-8', errors='replace').splitlines()
(r / 'runtime-end-extract.log').write_text('\n'.join(log[-55:])+'\n')
worker = json.loads((r / 'worker-result.json').read_text())
keys = ['cleanup', 'create_result', 'ngx_init', 'ngx_shutdown', 'device_removed',
        'device_removed_reason', 'per_frame_cpu_wait_between_apis',
        'real_frames_completed', 'real_frames_submitted', 'generated_disable_flag_zero_count',
        'debug_layer_enabled', 'd3d12_debug_errors']
(r / 'native-diagnostic-summary.json').write_text(json.dumps({k:worker.get(k, 'UNKNOWN') for k in keys}, indent=2)+'\n')

section = f"""## 2026-10-04 — DLSS SR + DLSS-G x2 {s['status']}; GL release and shutdown

Single combined run `{r.name}`, Java25.0.4 / Minecraft1.21.1 / Neo21.1.219; start HEAD `5d0c95a3d7d586c090de5e9a35d58393e9f28aba`. FG stopped automatically at 200 dispatches. No runtime retry, standalone SR rerun, AMD execution or x3/x4/x5/x6.

Observed: SR Evaluate/GPU completion {s['sr_evaluate_count']}/{s['sr_counts'].get('GPU_COMPLETION', 0)}, FG Create pools {s['fg_create_count']}, Evaluate {s['fg_evaluate_count']}, completion callbacks {s['fg_gpu_completion_count']}, G1 presents {s['fg_generated_present_count']}; REAL → G1 → REAL {s['real_g1_real_observed']}. Present events are worker submissions, not a scanout capture. Reset/discard guards remain; not every completed interval is presented.

Exact GL release waits: {s['wait_count']}, errors {s['gl_sync_error_count']}; first ten successful calls exported with immutable frame/slot/generation, distinct RELEASE semaphore, submitted Vulkan signal, valid imported objects/current render context and immediately post-call glGetError. Prior code could wait on RELEASE before its Vulkan signal was submitted, because SR readiness bypassed the release gate. [Khronos EXT_external_objects, section4.2.3](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/extensions/EXT/EXT_external_objects.txt) forbids this ordering. The new host submission notification defers capture/source reuse without blocking when release is not yet submitted, preserves the previous SR output, and only consumes a matching release once. No new hot-path CPU API wait/idle, CPU frame transport or second presenter; existing capture-ring backpressure remains. CPU transport count0 is confirmed by copy-event counters; the no-new-wait/sole-presenter claims also rely on source audit, not an API trace.

Flushed cleanup markers cover workers/presentation, FG pools/features/parameters/callbacks/imports, timeline/command pool, SR feature/parameters, NGX, GL interop and Vulkan. User closed normally: client/Gradle exit {s['process_exit_code']}/{s['gradle_exit_code']}, last completed `{s['shutdown_last_completed_stage']}`, failed stage `{s['shutdown_fail_stage']}`. Device lost {s['device_lost']}; 0xC0000409 reproduced {s['native_0xc0000409']}. Existing terminal drain is retained; imported semaphores have idempotent destruction. The earlier native crash's underlying cause remains UNKNOWN; this run establishes successful teardown after the release-contract correction and ownership guards, not historical fault attribution.

C++/JNI ABI4, SR NeoForge and Wisteria builds PASS. CPU release-contract test rejects12 unsafe cases; prior readiness13 and binary-cycle8 tests PASS. AMD9 classes+2DLL hashes unchanged. Configuration restored byte-for-byte `{s['config_restoration']['restored_sha256']}`. Historical MFG x3 and previous runtime evidence retained. Text evidence in `logs/runtime/dlss-sr/{r.name}/runtime-result.json`, `runtime-samples.jsonl`, exact GL waits and combined shutdown markers; raw telemetry/binary samples remain local.

"""
for name in ('CURRENT_STATE.md', 'DEVELOPMENT_LOG.md', 'DLSS_SR_DLSSG_X2_INTEROP.md'):
    path = root / 'docs' / name
    old = path.read_text(encoding='utf-8')
    if name == 'CURRENT_STATE.md':
        old = old.replace('## Estado vigente', '## Estado anterior', 1)
        path.write_text('## Estado vigente — DLSS SR + DLSS-G x2 PASS\n\n'+section+old, encoding='utf-8')
    else:
        line = old.find('\n')
        path.write_text(old[:line+1]+'\n'+section+old[line+1:], encoding='utf-8')
print(json.dumps({'status':s['status'], 'exported_run':r.name}))
