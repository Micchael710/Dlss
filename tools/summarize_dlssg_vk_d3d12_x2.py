"""Read-only GPU evidence reduction. Capabilities never contribute to PASS."""
import hashlib
import json
import sys
from pathlib import Path
from dlssg_log_evidence import empty_kernel_diagnostic

EXPECTED_INPUT_HASHES = {
    'A': '9285dff3162262072b299c4cbe94b40e9e51280c27ef9f7e38a08abe730cc140',
    'B': '4acff001b7f04ce1ce503314fbdd0dfadc6f2cf87d843dcf12365319bf387baa',
    'sentinel': '80ebb6cf65541911010912a1a2d2d35cb683f935d1f0b15dc8748f84d473c586',
    'depth': '7fea35808b0f7d554be9857f6a8e416b386e79d962c12987801e76e384477efc',
    'motion': '3bb90bb24e18c2acd0bb61772f0c8725d629bda4bb2bf1c24c553c52a38eb895',
}


def gates(worker, manifest, hashes, kernel_status, installed):
    result = {
        'external_loader': worker.get('external_loader_ready') == 'LOADED',
        'backend_install_observed': installed,
        'same_gpu': worker.get('same_gpu') == 'PASS',
        'ngx_init': worker.get('ngx_init') == 'PASS',
        'createfeature': worker.get('d3d12_createfeature') == 'PASS',
        'handle': worker.get('feature_handle') not in (None, '', '0', '0000000000000000', '0x0'),
        'kernels': kernel_status == 'PASS_OBSERVED',
        'warmup': worker.get('d3d12_warmup') == 'PASS',
        'evaluate': worker.get('d3d12_evaluate') == 'PASS',
        'gpu_submit': worker.get('target_submit_result') == 0,
        'gpu_completion': worker.get('target_completion_result') == 0 and worker.get('gpu_completion') == 'PASS',
        'input_write': worker.get('vulkan_input_write') == 'PASS',
        'fixture': worker.get('fixture_readback_valid') is True and all(hashes.get(k) == v for k, v in EXPECTED_INPUT_HASHES.items()),
        'sync_vk_dx': worker.get('vulkan_to_d3d12_gpu_sync') == 'PASS' and worker.get('vulkan_input_ready_signal_value') == 9 and worker.get('d3d12_input_ready_wait_value') == 9,
        'sync_dx_vk': worker.get('d3d12_to_vulkan_gpu_sync') == 'PASS' and worker.get('d3d12_dlssg_complete_signal_value') == 10 and worker.get('vulkan_dlssg_complete_wait_value') == 10,
        'vk_readback': worker.get('vulkan_g1_readback') == 'PASS',
        'output_enabled': worker.get('output_disable_interpolation') == 0,
        'distinct_output': hashes.get('G1') is not None and all(hashes.get(k) is not None and hashes['G1'] != hashes[k] for k in ('A', 'B', 'sentinel')),
        'temporal_content': worker.get('fixture_temporal_content_valid') is True and worker.get('d3d12_output') == 'PASS',
        'transport_cpu_zero': worker.get('bridge_transport_cpu_copy_count') == 0,
        'no_cpu_handoff_wait': worker.get('cpu_wait_between_apis') == 'NO',
        'no_device_lost': worker.get('device_lost') is False and worker.get('device_removed') is False and worker.get('device_removed_reason') == '0x00000000',
        'debug_clean': worker.get('d3d12_debug_errors') == 0 and worker.get('vulkan_validation_errors') == 0,
        'evidence_checkpoint': not worker.get('evidence_checkpoint_failed', False),
        'worker_complete': manifest.get('worker_exit_code') == 0 and manifest.get('worker_timeout') is False,
        'worker_checks': worker.get('end_to_end_worker') == 'PASS_PENDING_KERNEL_AND_FINAL_EVIDENCE_REVIEW',
    }
    for resource in ('A', 'B', 'DEPTH', 'MV', 'G1'):
        result[resource + '_shared'] = worker.get(resource + '_shared') == 'PASS'
        result[resource + '_same_allocation'] = worker.get(resource + '_same_allocation') == 'YES'
    return result


def self_test():
    # Capability-only evidence must fail every actual generation gate.
    caps = {'adapted_reported_available': 1, 'adapted_reported_max': 5,
            'adapted_reported_feature_init_result': 1}
    caps_gates = gates(caps, {}, {}, 'NOT_OBSERVED', False)
    assert not all(caps_gates.values())
    assert all(caps_gates[k] is False for k in ('createfeature', 'kernels', 'evaluate', 'gpu_completion', 'distinct_output', 'temporal_content'))
    assert empty_kernel_diagnostic({'bytes': 0, 'entry': '', 'routed': False, 'shader': '0x0'})
    assert not empty_kernel_diagnostic({'bytes': 1234, 'entry': 'Kernel_BlendCandidatesFused', 'routed': True, 'shader': '0x1'})
    worker = {'external_loader_ready': 'LOADED', 'same_gpu': 'PASS', 'ngx_init': 'PASS',
              'd3d12_createfeature': 'PASS', 'feature_handle': '0x1', 'd3d12_warmup': 'PASS',
              'd3d12_evaluate': 'PASS', 'target_submit_result': 0, 'target_completion_result': 0,
              'gpu_completion': 'PASS', 'vulkan_input_write': 'PASS', 'fixture_readback_valid': True,
              'vulkan_to_d3d12_gpu_sync': 'PASS', 'vulkan_input_ready_signal_value': 9, 'd3d12_input_ready_wait_value': 9,
              'd3d12_to_vulkan_gpu_sync': 'PASS', 'd3d12_dlssg_complete_signal_value': 10, 'vulkan_dlssg_complete_wait_value': 10,
              'vulkan_g1_readback': 'PASS', 'output_disable_interpolation': 0,
              'fixture_temporal_content_valid': True, 'd3d12_output': 'PASS',
              'bridge_transport_cpu_copy_count': 0, 'cpu_wait_between_apis': 'NO',
              'device_lost': False, 'device_removed': False, 'device_removed_reason': '0x00000000',
              'd3d12_debug_errors': 0, 'vulkan_validation_errors': 0,
              'end_to_end_worker': 'PASS_PENDING_KERNEL_AND_FINAL_EVIDENCE_REVIEW'}
    for name in ('A', 'B', 'DEPTH', 'MV', 'G1'):
        worker[name + '_shared'] = 'PASS'
        worker[name + '_same_allocation'] = 'YES'
    manifest = {'worker_exit_code': 0, 'worker_timeout': False}
    hashes = {**EXPECTED_INPUT_HASHES, 'G1': 'different-output'}
    assert all(gates(worker, manifest, hashes, 'PASS_OBSERVED', True).values())
    for key, value in [('d3d12_input_ready_wait_value', 8), ('bridge_transport_cpu_copy_count', 1),
                       ('device_removed', True), ('fixture_temporal_content_valid', False), ('G1_same_allocation', 'NO')]:
        assert not all(gates({**worker, key: value}, manifest, hashes, 'PASS_OBSERVED', True).values())
    for duplicate in ('A', 'B', 'sentinel'):
        assert not all(gates(worker, manifest, {**hashes, 'G1': hashes[duplicate]}, 'PASS_OBSERVED', True).values())
    assert not all(gates(worker, manifest, hashes, 'NOT_OBSERVED', True).values())
    assert not all(gates(worker, {**manifest, 'worker_timeout': True}, hashes, 'PASS_OBSERVED', True).values())
    print('CPU combined evidence gates PASS; no GPU or community binary loaded')


def reduce_run(out):
    def read(name, default):
        file = out / name
        return json.loads(file.read_text(encoding='utf-8-sig')) if file.exists() else default
    worker = read('worker-result.json', {})
    manifest = read('run-manifest.json', {})
    result = dict(worker)
    records = []
    for file in sorted((out / 'backend').rglob('*.jsonl')):
        for line_number, raw in enumerate(file.read_text(encoding='utf-8-sig', errors='replace').splitlines(), 1):
            try:
                item = json.loads(raw)
            except ValueError:
                continue
            records.append((item, {'file': str(file), 'line': line_number, 'raw': raw}))
    def event(item):
        return item.get('event', item.get('type', ''))
    installed = any(event(x) == 'backend_install' and x.get('status') == 0 for x, _ in records) and any(event(x) == 'install' and x.get('active') is True for x, _ in records)
    kernels = [(x, ref) for x, ref in records if event(x) == 'kernel_create' and not empty_kernel_diagnostic(x)]
    failed = [(x, ref) for x, ref in kernels if x.get('status') not in (None, 0, '0', '0x00000000')]
    kernel_status = 'FAIL_OBSERVED' if failed else 'PASS_OBSERVED' if kernels and all(x.get('status') in (0, '0', '0x00000000') for x, _ in kernels) and worker.get('d3d12_createfeature') == 'PASS' else 'NOT_OBSERVED'
    result.update({'d3d12_kernel_create': kernel_status, 'd3d12_kernel_count': len(kernels),
                   'd3d12_kernel_failure': [ref for _, ref in failed], 'kernel_evidence': [ref for _, ref in kernels],
                   'empty_kernel_diagnostics': [ref for x, ref in records if event(x) == 'kernel_create' and empty_kernel_diagnostic(x)],
                   'backend_install_observed': installed})
    hashes = {name: hashlib.sha256((out / (name + '.bin')).read_bytes()).hexdigest() if (out / (name + '.bin')).exists() else None for name in ('A', 'B', 'depth', 'motion', 'sentinel', 'G1')}
    conditions = gates(worker, manifest, hashes, kernel_status, installed)
    conditions['canonical_readback_hashes_match_worker'] = all(value is not None and value == worker.get(name + '_hash') for name, value in hashes.items())
    conditions['baseline_hashes_unchanged'] = manifest.get('baseline_before') == manifest.get('baseline_after') and bool(manifest.get('baseline_before'))
    resource_map = read('resource-map.json', [])
    conditions['five_dedicated_imports'] = len(resource_map) == 5 and {x.get('resource') for x in resource_map} == {'A', 'B', 'DEPTH', 'MV', 'G1'} and all(x.get('dedicated_allocation') is True and x.get('import_result') == 0 and x.get('bind_result') == 0 and x.get('memory_type_bits', 0) & (1 << x.get('memory_type_index', 32)) for x in resource_map)
    sync_map = read('sync-map.json', [])
    conditions['five_ordered_intervals'] = len(sync_map) == 5 and all(x.get('realFrameId') == f and x.get('count') == 1 and x.get('index') == 1 and x.get('Vulkan_input_ready_signal') == x.get('D3D12_input_ready_wait') == 2*f+1 and x.get('D3D12_DLSSG_complete_signal') == x.get('Vulkan_DLSSG_complete_wait') == 2*f+2 and x.get('CPU_ordering_between_APIs') is False for f, x in enumerate(sync_map))
    passed = all(conditions.values())
    result.update({'pass_conditions': conditions, 'run_directory': str(out), 'run_id': manifest.get('run_id'),
                   'canonical_readback_sha256': hashes, 'capabilities_are_generation_proof': False,
                   'vulkan_dlssg_d3d12_vulkan_x2': 'PASS' if passed else 'FAIL',
                   'dlssg_vulkan_sidecar_end_to_end': 'PASS' if passed else 'FAIL',
                   'minecraft_integration_ready': 'YES_FOR_NEXT_PHASE' if passed else 'NO',
                   'generated_count_confirmed': 1 if passed else 0,
                   'baseline_amd_modified': False, 'wisteria_modified': False,
                   'presentworker_modified': False, 'minecraft_launched': False,
                   'vulkan_dlssg_retested': False, 'mfg_higher_than_x2_tested': False,
                   'automatic_retries': 0, 'manifest': manifest})
    if not passed:
        if failed:
            result['if_fail_stage'] = 'KERNEL_CREATION'
        elif worker.get('if_fail_stage') in (None, 'NONE'):
            result['if_fail_stage'] = 'EVIDENCE_PERSISTENCE' if not conditions['canonical_readback_hashes_match_worker'] else 'VALIDATION'
        result['root_cause'] = worker.get('error', 'Required evidence gates missing: ' + ', '.join(k for k, v in conditions.items() if not v))
    else:
        result['if_fail_stage'] = 'NONE'
        result['root_cause'] = 'NONE'
    def write(name, data):
        (out / name).write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    write('result.json', result)
    write('capabilities.json', {'provenance': 'REPORTED_POTENTIALLY_HOOKED', 'generation_proof': False, **{k: v for k, v in worker.items() if k.startswith('adapted_reported_') or k == 'capability_query_result'}})
    write('hashes.json', {'readbacks': hashes, 'expected_fixture': EXPECTED_INPUT_HASHES, 'executable': manifest.get('executable'), 'runtime': manifest.get('stock_runtime'), 'external_component': manifest.get('community_component')})
    write('timings.json', {k: v for k, v in worker.items() if 'time' in k or k.endswith('_ms') or k.endswith('_hz')})
    print('RUN_DIRECTORY=' + str(out))
    print('VULKAN_DLSSG_D3D12_VULKAN_X2=' + result['vulkan_dlssg_d3d12_vulkan_x2'])
    print('IF_FAIL_STAGE=' + result.get('if_fail_stage', 'UNKNOWN'))


if __name__ == '__main__':
    if sys.argv[1:] == ['--self-test']:
        self_test()
    else:
        reduce_run(Path(sys.argv[1]).resolve())
