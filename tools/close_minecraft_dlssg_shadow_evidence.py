"""Close evidence for the single failed shadow attempt. No GPU execution/retry."""
from pathlib import Path
import hashlib, json, re, shutil, subprocess

ROOT = Path(__file__).resolve().parents[1]
RUN = ROOT / 'logs/runtime/minecraft-dlssg-x2/20261004-010600-001'
if (RUN / 'result.json').exists():
    raise SystemExit('Evidence already closed. No rewrite or config restoration repeated.')
GIT = 'C:/Users/micha/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/git/cmd/git.exe'
def write(name, obj):
    (RUN / name).write_text(json.dumps(obj, indent=2) + '\n', encoding='utf-8')
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
manifest = json.loads((RUN / 'manifest.json').read_text())
worker = json.loads((RUN / 'worker-result.json').read_text())
log = (RUN / 'launcher.log').read_text(errors='replace')
native_events = [json.loads(line) for line in (RUN / 'events.jsonl').read_text().splitlines() if line]
assert manifest['mode'] == 'shadow' and manifest['attempt_limit'] == 1
assert log.count('DLSSG_EXPERIMENT_FAIL stage=SESSION_INIT') == 1
assert 'D3D12CreateDevice=0x887a0007' in (RUN / 'events.jsonl').read_text()
assert worker['debug_layer_enabled'] is True
assert worker['external_loader_ready'] == 'LOADED'
assert 'BUILD SUCCESSFUL' in log and 'All dimensions are saved' in log
assert not (RUN / 'sample-0-G.bin').exists()
assert not any(e['type'] in ('evaluate', 'gpu_completion') for e in native_events)
baseline = {
 'builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.f0e41bf8.opengl-fg-backpressure.jar': '20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896',
 'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar': 'ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53',
 'wisteria/native/build/fsr-windows-x64/wisteria_fsr_bridge.dll': 'b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b',
}
for path, expected in baseline.items():
    assert sha(ROOT / path) == expected, path
for item in manifest['artifacts'].values():
    assert sha(ROOT / item['path']) == item['sha256']
config = ROOT / 'runtime-baseline/instance/config/super_resolution/config.toml'
manifest['test_config_at_shutdown_sha256'] = sha(config)
shutil.copy2(RUN / 'original-test-config.toml', config)
assert sha(config) == manifest['original_test_config_sha256']
manifest.update(status='CLOSED_FAIL_NO_RETRY', minecraft_pid=1962152,
                implementation_commit=subprocess.check_output([GIT, 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                isolated_config_restored=True, final_config_sha256=sha(config),
                process_shutdown='ORDERLY_WORLD_SAVED_VULKAN_DESTROYED',
                presentation_attempts=0, runtime_attempts=1)
write('run-manifest.json', manifest)
write('baseline-integrity.json', baseline)
capture = []
for number, line in enumerate(log.splitlines(), 1):
    if 'SR_FG_INPUT frame=' in line or 'SR_FG_INPUT sealed metadata=' in line:
        capture.append(dict(source='launcher.log', line=number, raw=line, type='REAL_INPUT_CAPTURE'))
(RUN / 'frame-sequence.jsonl').write_text(''.join(json.dumps(e)+'\n' for e in capture), encoding='utf-8')
write('resource-pool.json', dict(status='NOT_CREATED', designed_slot_bound=8,
      bound_source='PRESENTATION_QUEUE_CAPACITY/(0+1)+2', actual_slots=0,
      shared_resource_handles_created=0, per_frame_handle_imports=0,
      reason='D3D12 device creation failed before pool/feature setup'))
write('timings.json', dict(status='NOT_MEASURED', input_vulkan_gpu_copy_ms=None,
      dlssg_gpu_ms=None, queue_delay_ms=None, added_display_latency_ms=None,
      reason='No DLSS-G submission or generated output in this attempt'))
(RUN / 'sync.jsonl').write_text(json.dumps(dict(type='NOT_REACHED', reason='D3D12_CREATE_DEVICE failed before shared fence creation'))+'\n', encoding='utf-8')
(RUN / 'errors.jsonl').write_text(json.dumps(dict(stage='SESSION_INIT/D3D12_CREATE_DEVICE',
       api='D3D12CreateDevice', hresult='0x887A0007', symbolic='DXGI_ERROR_DEVICE_RESET',
       retry_count=0, ngx_init_reached=False, source='events.jsonl'))+'\n', encoding='utf-8')
selected = [line for line in log.splitlines() if 'DLSSG' in line or 'Caused by: java.lang.IllegalStateException: D3D12CreateDevice' in line or 'backend negotiation: fg=null' in line]
(RUN / 'provider.log').write_text('\n'.join(selected)+'\n', encoding='utf-8')
shutil.copy2(RUN / 'ngx.log', RUN / 'ngx-callback.log')
result = {
 'run_id': manifest['run_id'], 'artifacts': manifest['artifacts'],
 'MINECRAFT_DLSSG_SHADOW_X2': 'FAIL', 'MINECRAFT_DLSSG_X2_PRESENTATION': 'NOT_RUN_SHADOW_GATE_FAILED',
 'MINECRAFT_LAUNCH': 'PASS', 'WORLD_LOAD': 'PASS',
 'WISTERIA_DLSSG_PROVIDER_REGISTRATION': 'PASS', 'OWN_JNI_LOAD': 'PASS',
 'EXTERNAL_LOADER': 'PASS_PUBLIC_LOAD_ONLY', 'SAME_GPU_SELECTION': 'PASS_OBSERVED_UUID_LUID',
 'D3D12_DEVICE_CREATE': 'FAIL_0x887A0007_DXGI_ERROR_DEVICE_RESET',
 'NGX_INIT': 'NOT_REACHED', 'CREATE_FEATURE': 'NOT_REACHED', 'KERNEL_CREATE': 'NOT_REACHED',
 'EVALUATE': 'NOT_REACHED', 'GPU_COMPLETION': 'NOT_REACHED', 'G1_OUTPUT': 'NOT_PRODUCED',
 'SHADOW_REAL_FRAMES': 0, 'SHADOW_GENERATED_FRAMES': 0, 'SHADOW_EVALUATE_FAILURES': 0,
 'SHADOW_INIT_FAILURES': 1, 'SHADOW_DEVICE_LOST': 'D3D12_RESET_REPORTED_AT_CREATE;NO_VULKAN_DEVICE_LOST_OBSERVED',
 'capture_metadata_samples': len(capture), 'counts_scope': 'accepted DLSS-G jobs; not all Minecraft rendered/presented frames',
 'FUNCTIONAL_PASS': 'NO', 'VISUAL_QUALITY': 'NOT_ASSESSED_DLSSG',
 'CRASH': False, 'ORDERLY_SHUTDOWN': True, 'NO_AUTOMATIC_RETRY': True,
 'PROVIDER_UNHEALTHY_LATCH': 'PASS_OBSERVED;FG negotiation became null;no silent AMD fallback',
 'D3D12_DEBUG_ERRORS': 'UNAVAILABLE_NO_DEVICE_INFO_QUEUE',
 'VULKAN_VALIDATION_AVAILABLE': 'NOT_ENABLED_IN_THIS_PROFILE', 'VULKAN_VALIDATION_ERRORS': 'NOT_MEASURED',
 'DEVICE_REMOVED_REASON': 'UNAVAILABLE_NO_CREATED_ID3D12DEVICE',
 'PER_FRAME_CPU_WAIT_BETWEEN_APIS': 'NOT_EXERCISED;implementation uses GPU waits',
 'CPU_TRANSPORT_COPY_COUNT': 0, 'INPUT_GPU_COPY_COUNT_EXECUTED': 0, 'OUTPUT_GPU_COPY_COUNT_EXECUTED': 0,
 'D3D12_PRESENT_CALLS': 0, 'VKQUEUEPRESENT_CALLS_FROM_DLSSG_PROVIDER': 0,
 'PRESENTWORKER_STILL_SOLE_PRESENT_OWNER': True,
 'AMD_BASELINE_MODIFIED': False, 'DIRECT_VULKAN_DLSSG_RETESTED': False,
 'MFG_HIGHER_THAN_X2_TESTED': False, 'DLSS_SUPER_RESOLUTION_MODIFIED': False,
 'IF_FAIL_STAGE': 'SESSION_INIT/D3D12_CREATE_DEVICE',
 'ROOT_CAUSE': 'Confirmed HRESULT 0x887A0007. Suspected late process-wide EnableDebugLayer from reused isolated helper; pre-existing D3D12 device not proven. No NGX kernels/FG commands ran.',
 'CAUSE_CONFIDENCE': 'HIGH_FAILURE_STAGE;MEDIUM_DEBUG_LAYER_HYPOTHESIS;UNDERLYING_DEVICE_RESET_NOT_PROVEN',
 'next_step': 'Separate embedded initialization from standalone debug bootstrap; never toggle D3D12 debug layer late. Review evidence before another explicitly scoped runtime attempt. No correction/retry performed in this phase.',
 'windows_error_reference': 'https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error',
 'debug_layer_reference': 'https://learn.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nf-d3d12sdklayers-id3d12debug-enabledebuglayer',
}
write('result.json', result)
print(json.dumps(result, indent=2))
