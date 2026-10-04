"""Close existing failed MFG evidence offline. Never launches, retries, or declares PASS."""
from pathlib import Path
import collections, hashlib, json, re, sys
from dlssg_log_evidence import empty_kernel_diagnostic

ROOT = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
assert out.is_relative_to(ROOT / 'logs/runtime')
assert not (out / 'result.json').exists(), 'Closed results are immutable'

def text(name):
    p = out / name
    return p.read_text(encoding='utf-8-sig', errors='replace') if p.exists() else ''

def records(name):
    result = []
    for line, raw in enumerate(text(name).splitlines(), 1):
        if raw.strip():
            result.append({**json.loads(raw), 'source': name, 'line': line})
    return result

def fields(detail):
    return dict(re.findall(r'(\w+)=([^\s;]+)', detail))

def write(name, value):
    (out / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')

def jsonl(name, values):
    (out / name).write_text(''.join(json.dumps(v) + '\n' for v in values), encoding='utf-8')

manifest = json.loads(text('manifest.json'))
worker = json.loads(text('worker-result.json'))
log = text('launcher.log')
assert 'Game crashed! Crash report saved to:' in log, 'Requires observed Minecraft crash'
assert 'GPU disable metadata unavailable' in log, 'Only closes this observed failure stage'
count = manifest['requested_count']
provider = records('provider-events.jsonl')
by = collections.defaultdict(list)
for event in provider:
    by[event['type']].append({**event, 'fields': fields(event.get('detail', ''))})
backend = []
for p in sorted((out / 'backend').glob('*.jsonl')):
    backend.extend(records(p.relative_to(out).as_posix()))
kernels = [e for e in backend if e.get('event') == 'kernel_create' and not empty_kernel_diagnostic(e)]
evals = {(int(e['fields']['realFrameId']), int(e['fields']['index'])): e for e in by['evaluate']}
completions = {(int(e['fields']['realFrameId']), int(e['fields']['index'])): e for e in by['completion']}
groups = []
for real_id in sorted({key[0] for key in evals}):
    entries = []
    for index in range(1, count + 1):
        ev = evals.get((real_id, index)); completion = completions.get((real_id, index))
        entries.append({'index': index, 'evaluate': ev, 'completion': completion,
                        'pixel_content': 'NOT_MEASURED', 'presentation': 'UNKNOWN'})
    groups.append({'realFrameAId': real_id - 1, 'realFrameBId': real_id, 'outputs': entries})
unknown_flags = [e for e in by['completion'] if int(e['fields']['disable']) < 0]
assert unknown_flags, 'Requires observed unavailable completion metadata'
flag_eligible = [e for key, e in completions.items()
                if e['fields']['disable'] == '0' and key in evals
                and evals[key]['fields']['reset'] == '0'
                and evals[key]['fields']['result'] == '0x00000001'
                and evals[key]['fields']['done'] == e['fields']['done']]
sequence = text('frame-sequence.log')
present_lines = [line for line in sequence.splitlines() if ' PRESENT ' in line]
assert not present_lines, 'This failure reviewer cannot validate presentation'
clean_worker = {k: v for k, v in worker.items() if not k.startswith('modules')}
write('worker-summary.json', clean_worker)
jsonl('frame-sequence.jsonl', groups)
jsonl('sync.jsonl', by['vulkan_d3d12_submit'] + by['completion'])
write('resource-pool.json', {'images': by['resource_pool'],
      'observed_shared_images': len(by['resource_pool']),
      'images_per_slot': 4 + count, 'observed_slots': len(by['resource_pool']) // (4 + count),
      'expected_slots': manifest['pool_slots'], 'generated_outputs_per_slot': count})
jsonl('errors.jsonl', [{'source': 'launcher.log', 'line': i, 'raw': line}
      for i, line in enumerate(log.splitlines(), 1)
      if 'GPU disable metadata unavailable' in line or 'Application-managed frame presenter failed' in line
      or 'Game crashed!' in line])
write('timings.json', {'scope': 'per-index D3D12 GPU timestamp pairs; startup-only, not FPS benchmark',
      'gpu_ms': [dict(realFrameId=e['fields']['realFrameId'], index=e['fields']['index'],
                      milliseconds=float(e['fields']['DLSSG_GPU_ms'])) for e in by['completion']],
      'real_fps': 'NOT_MEASURED', 'presented_fps': 'NOT_MEASURED',
      'dropped': 'UNKNOWN', 'late': 'UNKNOWN',
      'presentation_evidence': 'Java writer buffered all records; frame-sequence.log empty after crash'})
(out / 'provider.log').write_text('Derived from provider-events.jsonl; not a new runtime log.\n' +
      '\n'.join(e.get('detail', '') for e in provider) + '\n', encoding='utf-8')
(out / 'ngx-callback.log').write_text(text('ngx.log'), encoding='utf-8')
baseline = {path: {'expected_sha256': expected,
            'actual_sha256': hashlib.sha256((ROOT / path).read_bytes()).hexdigest()}
            for path, expected in manifest['baseline_sha256'].items()}
assert all(v['expected_sha256'] == v['actual_sha256'] for v in baseline.values())
write('baseline-integrity.json', baseline)
runtime_match = re.search(r'Java Version: ([^,\r\n]+)', log)
runtime_java = runtime_match.group(1) if runtime_match else 'UNKNOWN'
result = {
    f'MINECRAFT_DLSSG_X{count + 1}': 'FAIL', 'run_id': manifest['run_id'],
    'failure_stage': 'OUTPUT_VALIDITY_METADATA', 'observed_crash': True,
    'actual_runtime_java': runtime_java, 'required_runtime_java': 25,
    'runtime_conforming': runtime_java.startswith('25.'), 'java25_graphic_result': 'NOT_RUN',
    'failure_causal_relation_to_java_version': 'UNKNOWN',
    'requested_count': count, 'external_requested_max': worker.get('external_requested_max'),
    'raw_reported_max': worker.get('raw_reported_multiframecountmax'),
    'raw_getter_result': worker.get('raw_max_getter_result'),
    'capability_provenance': worker.get('capability_provenance'),
    'NGX_INIT': worker.get('ngx_init'), 'CREATEFEATURE': worker.get('minecraft_create_feature'),
    'named_kernel_create_events': len(kernels),
    'kernel_create_success': bool(kernels) and all(e.get('status') in (0, '0', '0x00000000') for e in kernels),
    'evaluate_count': len(evals), 'completion_count': len(completions),
    'real_groups_completed': len({k[0] for k in completions}),
    'evaluate_success_by_index': {str(i): sum(e['fields']['result'] == '0x00000001'
         for key, e in evals.items() if key[1] == i) for i in range(1, count + 1)},
    'disable_values_by_index': {str(i): [int(e['fields']['disable'])
         for key, e in completions.items() if key[1] == i] for i in range(1, count + 1)},
    'flag_eligible_completion_count': len(flag_eligible),
    'counter_scope': 'GPU completed, success Evaluate, disable0, non-reset, nonzero allocated image; no pixel proof',
    'content_validated_output_count': 0, 'bounded_readbacks': len(by['output_sample']),
    'presentation_count': 'UNKNOWN', 'temporal_order': 'UNKNOWN', 'present_order': 'UNKNOWN',
    'frame_sequence_log_bytes': (out / 'frame-sequence.log').stat().st_size,
    'native_submit_cpu_wait': any(e['fields'].get('cpuWait') != 'false' for e in by['vulkan_d3d12_submit']),
    'device_removed': worker.get('device_removed', 'UNKNOWN'),
    'device_removed_reason': worker.get('device_removed_reason', 'UNKNOWN'),
    'AMD_baseline_unchanged': True, 'x4': 'NOT_RUN', 'x5': 'NOT_RUN',
    'root_cause': 'UNKNOWN: index2 flag matches 0xffffffff sentinel; mixed [0,-1] excludes callback catch-all read/fence error, which sets all flags to -1',
    'next_step': 'Audit public MFG disable-output contract and completion metadata; next authorized runtime must use Java25',
    'evidence_sha256': {p.relative_to(out).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in out.rglob('*') if p.is_file() and p.suffix in ('.log', '.jsonl')},
}
write('result.json', result)
manifest.update(status='CLOSED_FAILED_OUTPUT_METADATA', actual_runtime_java=runtime_java,
                required_runtime_java=25, runtime_conforming=result['runtime_conforming'],
                raw_reported_max=result['raw_reported_max'], capability_provenance=result['capability_provenance'])
write('run-manifest.json', manifest)
print(json.dumps({k: v for k, v in result.items() if k != 'evidence_sha256'}, indent=2))
