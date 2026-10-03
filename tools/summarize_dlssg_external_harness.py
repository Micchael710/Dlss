"""Reduce preserved harness evidence; never infer generated frames from capabilities."""
import hashlib
import json
import re
import sys
from pathlib import Path

out = Path(sys.argv[1]).resolve()

def read_json(name):
    p = out / name
    try:
        return json.loads(p.read_text(encoding='utf-8-sig'))
    except (OSError, ValueError) as error:
        return {'parse_error': str(error)}

worker = read_json('worker-result.json')
manifest = read_json('run-manifest.json')
coordinator = read_json('coordinator.json')
stock = read_json('stock-reference.json').get('stock', {})
result = dict(worker)
result.update({'run_directory': str(out), 'api': 'Vulkan', 'requested_count': 1,
               'requested_index': 1, 'coordinator': coordinator,
               'stock_original_available': stock.get('available', {}).get('value'),
               'stock_original_max': stock.get('multi_frame_count_max', {}).get('value'),
               'stock_getter_results': {k: stock.get(k) for k in
                    ('available', 'feature_init_result', 'needs_updated_driver', 'multi_frame_count_max')},
               'stock_reference_reused_not_rerun': True,
               'stock_gpu_driver_match': stock.get('device_uuid') == worker.get('device_uuid') and
                                         stock.get('driver_raw') == worker.get('driver_raw'),
               'adapted_original_available': None, 'adapted_original_max': None,
               'adapted_original_provenance': 'NOT_OBSERVABLE_AFTER_EXTERNAL_LOAD',
               'baseline_amd_modified': False, 'minecraft_launched': False,
               'd3d12_launched': False, 'need_d3d12_sidecar': 'NO',
               'sidecar_reason': 'Not activated; isolated Vulkan outcome must first be interpreted',
               'manifest': manifest})

# Logs are preserved unchanged, and every interpretation includes an exact source line.
records = []
for path in sorted((out / 'backend').rglob('*.jsonl')):
    for number, line in enumerate(path.read_text(encoding='utf-8-sig', errors='replace').splitlines(), 1):
        try:
            item = json.loads(line)
        except ValueError:
            continue
        records.append((item, {'file': str(path), 'line': number, 'raw': line}))

def event(item):
    return item.get('event', item.get('type', ''))

install_status = [ref for item, ref in records if event(item) == 'backend_install' and item.get('status') == 0]
active_install = [ref for item, ref in records if event(item) == 'install' and item.get('active') is True]
result['backend_install_evidence'] = install_status + active_install
if install_status and active_install:
    result['backend_install_observation'] = 'OBSERVED_INSTALLED_ACTIVE_NOT_GENERATION_PROOF'
else:
    result['backend_install_observation'] = 'NOT_OBSERVED' if not records else 'LOGGED_NO_CONFIRMED_ACTIVE_INSTALL'

kernels = [(item, ref) for item, ref in records if event(item) == 'kernel_create']
result['kernel_evidence'] = [ref for _, ref in kernels]
result['vulkan_kernel_create'] = 'NOT_OBSERVED'
if kernels:
    statuses = [item.get('status') for item, _ in kernels]
    # Vulkan creation is not inferred from D3D12 kernel logs or route selection alone.
    result['vulkan_kernel_create'] = 'OBSERVED_LOGS_API_ATTRIBUTION_PENDING'
    if all(x in (0, '0', '0x00000000') for x in statuses) and worker.get('vulkan_createfeature') == 'PASS':
        result['vulkan_kernel_create'] = 'OBSERVED_SUCCESS_DURING_VULKAN_WORKER'

callback = out / 'ngx-callback.log'
callback_lines = callback.read_text(encoding='utf-8', errors='replace').splitlines() if callback.exists() else []
arch = [{'file': str(callback), 'line': n, 'raw': line}
        for n, line in enumerate(callback_lines, 1)
        if re.search(r'architecture.*0x170.*0x190|GPU architecture.*0x170|expects.*0x190', line, re.I)]
result['architecture_evidence'] = arch
if worker.get('create_result') == '0xbad0000b' and arch:
    result['if_fail_stage'] = 'architecture gate'
    result['cause_confidence'] = 'HIGH: CreateFeature=0xBAD0000B plus NGX physical/minimum architecture rejection'
elif worker.get('vulkan_createfeature') == 'FAIL':
    result['if_fail_stage'] = 'feature creation'
    result['cause_confidence'] = 'HIGH_STAGE_ONLY; root cause requires explicit callback/backend evidence'
if worker.get('device_lost'):
    result['if_fail_stage'] = 'synchronization'
    result['cause_confidence'] = 'HIGH_DEVICE_LOST_OBSERVED; kernel cause unproven'
if coordinator.get('timeout') or coordinator.get('worker_exit_code', 0) not in (0, 1):
    result['worker_abnormal_exit'] = True
    result['cause_confidence'] = 'PARTIAL_EVIDENCE_WORKER_EXIT; no automatic retry'

hashes = {}
for label in ('A', 'B', 'G1', 'sentinel'):
    binary = out / (label + '.bin')
    hashes[label] = hashlib.sha256(binary.read_bytes()).hexdigest() if binary.exists() else None
    if hashes[label] is not None and worker.get(label + '_hash') != hashes[label]:
        result['evidence_hash_mismatch'] = True
    # PPM is already an inspectable image; PNG is an additional compact preview.
    ppm = out / (label + '.ppm')
    if ppm.exists():
        try:
            from PIL import Image
            with Image.open(ppm) as img:
                img.save(out / (label + '.png'))
        except ImportError:
            pass
result['canonical_readback_sha256'] = hashes

pass_conditions = {
    'fixture_roundtrip': worker.get('fixture_readback_valid') is True,
    'ngx_init': worker.get('ngx_init') == 'PASS',
    'create': worker.get('vulkan_createfeature') == 'PASS',
    'feature_handle': worker.get('feature_handle') not in (None, '', '0', '0000000000000000'),
    'evaluate': worker.get('vulkan_evaluate') == 'PASS',
    'submit': worker.get('target_submit_result') == 0,
    'completion': worker.get('target_completion_result') == 0 and worker.get('gpu_completion') == 'PASS',
    'no_device_lost': worker.get('device_lost') is False,
    'output_disable_false': worker.get('output_disable_interpolation') == 0,
    'temporal_content': worker.get('fixture_temporal_content_valid') is True,
    'output_hashes_distinct': hashes['G1'] is not None and None not in hashes.values() and
                             all(hashes['G1'] != hashes[x] for x in ('A', 'B', 'sentinel')),
    'output_readback': worker.get('vulkan_output') == 'PASS',
    'no_validation_error': worker.get('validation_errors') == 0,
    'evidence_integrity': not result.get('evidence_hash_mismatch') and 'parse_error' not in worker,
    'worker_completed': coordinator.get('worker_exit_code') == 0 and not coordinator.get('timeout'),
}
result['pass_conditions'] = pass_conditions
result['dlssg_sm86_vulkan_x2'] = 'PASS' if all(pass_conditions.values()) else 'FAIL'
result['generated_count_confirmed'] = 1 if all(pass_conditions.values()) else 0
result['capabilities_are_generation_proof'] = False
result['effective_runtime_candidates'] = [p for p in worker.get('modules_final', [])
                                          if 'nvngx_dlssg' in p.lower() or 'sm86_backend' in p.lower()]
result['runtime_selection_log_evidence'] = [{'file': str(callback), 'line': n, 'raw': line}
    for n, line in enumerate(callback_lines, 1)
    if any(k in line.lower() for k in ('nvngx_dlssg', 'selected', 'snippet', 'library loaded'))]
(out / 'result.json').write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')

keys = [('EXTERNAL_LOADER_READY', 'external_loader_ready'),
        ('BACKEND_INSTALL_OBSERVATION', 'backend_install_observation'), ('NGX_INIT', 'ngx_init'),
        ('ADAPTED_REPORTED_AVAILABLE', 'adapted_reported_available'),
        ('ADAPTED_REPORTED_FEATURE_INIT_RESULT', 'adapted_reported_feature_init_result'),
        ('ADAPTED_REPORTED_MAX', 'adapted_reported_max'),
        ('VULKAN_CREATEFEATURE', 'vulkan_createfeature'), ('VULKAN_KERNEL_CREATE', 'vulkan_kernel_create'),
        ('VULKAN_EVALUATE', 'vulkan_evaluate'), ('GPU_COMPLETION', 'gpu_completion'),
        ('VULKAN_OUTPUT', 'vulkan_output'), ('A_HASH', 'A_hash'), ('B_HASH', 'B_hash'),
        ('G1_HASH', 'G1_hash'), ('SENTINEL_HASH', 'sentinel_hash'),
        ('GENERATED_COUNT_CONFIRMED', 'generated_count_confirmed'),
        ('DLSSG_SM86_VULKAN_X2', 'dlssg_sm86_vulkan_x2'), ('IF_FAIL_STAGE', 'if_fail_stage'),
        ('CAUSE_CONFIDENCE', 'cause_confidence'), ('NEED_D3D12_SIDECAR', 'need_d3d12_sidecar')]
summary = '\n'.join(f'{title}={result.get(key, "NOT_RUN")}' for title, key in keys)
(out / 'summary.txt').write_text(summary + '\n', encoding='utf-8')
print(summary)
