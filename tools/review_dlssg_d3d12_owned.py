"""Validate preserved CPU readbacks/evidence only; never launches a GPU test."""
import hashlib
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
run = Path(sys.argv[1]).resolve()
read = lambda name: json.loads((run / name).read_text(encoding='utf-8-sig'))
r, manifest = read('result.json'), read('run-manifest.json')
for path, digest in manifest['source_sha256'].items():
    assert hashlib.sha256((root / path).read_bytes()).hexdigest() == digest
assert hashlib.sha256((root / 'experiments/dlssg-crossapi-interop/build/dlssg_d3d12_owned.exe').read_bytes()).hexdigest() == manifest['exe_sha256']
assert manifest['worker_exit_code'] == 0 and not manifest['worker_timeout']
expected = {'vulkan_A.bin': bytes([255, 0, 0, 255]) * (256 * 256),
            'd3d12_B.bin': bytes([0, 0, 255, 255]) * (256 * 256)}
hashes = {}
for name, pixels in expected.items():
    actual = (run / name).read_bytes()
    assert actual == pixels
    hashes[name] = {'expected_sha256': hashlib.sha256(pixels).hexdigest(),
                    'actual_sha256': hashlib.sha256(actual).hexdigest(),
                    'bytes': len(actual), 'exact_pixels_match': True}
assert r['d3d12_source_hash'] == r['vulkan_imported_readback_hash'] == hashes['vulkan_A.bin']['actual_sha256']
assert r['vulkan_source_hash'] == r['d3d12_imported_readback_hash'] == hashes['d3d12_B.bin']['actual_sha256']
bits = r['vk_image_memory_type_bits'] & r['vk_handle_memory_type_bits']
assert bits == r['vk_memory_type_bits'] and bits & (1 << r['vk_selected_memory_type'])
required = ('same_gpu', 'resource_handle_created', 'vk_import_memory', 'vk_bind_image_memory',
            'handle_ownership_valid', 'd3d12_to_vulkan_shared_resource',
            'vulkan_writes_d3d12_owned_resource', 'd3d12_fence_shared_handle',
            'vk_import_d3d12_fence', 'd3d12_to_vulkan_gpu_sync',
            'vulkan_to_d3d12_gpu_sync', 'd3d12_owned_vulkan_roundtrip', 'cross_api_interop')
assert all(r[k] == 'PASS' for k in required)
assert r['dedicated_allocation_used'] and r['vk_dedicated_only']
assert r['same_allocation_shared'] == 'YES' and r['bridge_transport_cpu_copy_count'] == 0
assert r['validation_cpu_readback_count'] == 2
assert not r['device_lost'] and r['device_removed_reason'] == 0
assert r['d3d12_debug_errors'] == r['vulkan_validation_errors'] == 0
baseline = {
    'builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.f0e41bf8.opengl-fg-backpressure.jar': '20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896',
    'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar': 'ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53',
    'wisteria/native/build/fsr-windows-x64/wisteria_fsr_bridge.dll': 'b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b',
}
for path, digest in baseline.items():
    assert hashlib.sha256((root / path).read_bytes()).hexdigest() == digest
def save(name, value):
    (run / name).write_text(json.dumps(value, indent=2), encoding='utf-8')
save('hashes.json', hashes)
save('handle-ownership.json', {
    'resource': {'creator': 'D3D12 CreateSharedHandle', 'importer': 'Vulkan vkAllocateMemory',
                 'import_consumes_handle': False, 'closer': 'application',
                 'close_stage': 'after successful vkAllocateMemory, before vkBindImageMemory',
                 'duplicate_handle_used': False, 'payload_references': 'D3D12 resource + imported VkDeviceMemory'},
    'fence': {'creator': 'D3D12 CreateSharedHandle', 'importer': 'vkImportSemaphoreWin32HandleKHR',
              'import_consumes_handle': False, 'closer': 'application',
              'close_stage': 'after successful permanent timeline import', 'duplicate_handle_used': False},
    'valid': True})
save('timings.json', {'cpu_harness_wall_ms': r['cpu_harness_wall_ms'],
    'method': 'steady_clock CPU; includes devices, resource creation, logging, hashing, validation readback and cleanup',
    'vulkan_gpu_ms': None, 'd3d12_gpu_ms': None, 'handoff_gpu_ms': None,
    'gpu_timestamps_instrumented': False, 'cross_api_clocks_combined': False,
    'is_bridge_latency_benchmark': False})
save('test-results.json', {'build': 'PASS', 'cpu_self_tests': manifest['cpu_self_tests'],
    'cpu_memory_type_intersection_and_empty_intersection': 'PASS',
    'independent_python_exact_pixel_validation': 'PASS', 'compiled_provenance_matches': True,
    'gpu_run_count': 1, 'baseline_unchanged': True, 'baseline_sha256': baseline})
save('reviewed-result.json', {**r, 'd3d12_owned_vulkan_interop': 'PASS',
    'dlssg_vulkan_sidecar_prerequisites': 'PASS_COLOR_RESOURCE_SYNC_SCOPE',
    'dlssg_executed_in_this_phase': False, 'mfg_higher_than_x2_tested': False,
    'vulkan_dlssg_retested': False, 'minecraft_launched': False,
    'amd_baseline_modified': False, 'wisteria_modified': False, 'presentworker_modified': False,
    'baseline_sha256': baseline, 'required_gate_checks': {k: True for k in required},
    'gpu_transport_copies': 0, 'gpu_validation_copies': 2,
    'source_hash_semantics': 'Independent CPU reference for GPU clears; no CPU upload creates shared image contents',
    'resource_state_protocol': {'d3d12_before_handoff': 'RENDER_TARGET -> COMMON',
        'vulkan_acquire': 'EXTERNAL/GENERAL -> graphics/TRANSFER_SRC_OPTIMAL or TRANSFER_DST_OPTIMAL',
        'vulkan_release': 'graphics/TRANSFER_* -> EXTERNAL/GENERAL',
        'd3d12_after_return': 'COMMON -> COPY_SOURCE -> COMMON'},
    'scope_limits': ['256x256 RGBA8_UNORM ALLOW_RENDER_TARGET, one allocation, two GPU clear/readback phases',
        'Khronos layer unavailable; zero observed debug-utils errors is not full Vulkan layer validation',
        'COMMON/GENERAL handoff observed on this driver; not a universal state/layout equivalence guarantee',
        'No DLSS-G, depth/MV formats, storage resources, repeated frames, pacing or latency benchmark'],
    'original_evidence_sha256': {name: hashlib.sha256((run / name).read_bytes()).hexdigest()
        for name in ('result.json', 'run-manifest.json', 'events.jsonl', 'resource-map.json', 'sync-map.json')}})
print(json.dumps({'d3d12_owned_vulkan_interop': 'PASS', 'exact_hashes': True,
                  'baseline_unchanged': True, 'gpu_runs': 1}))
