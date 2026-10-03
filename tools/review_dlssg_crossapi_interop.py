"""Read-only interpretation of one completed capability run; no GPU calls."""
import hashlib
import json
import sys
from pathlib import Path


def export_route(memory_features, sync_features):
    # Import support and exportFromImportedHandleTypes are not native export support.
    memory_bidirectional = memory_features & 6 == 6
    sync_bidirectional = sync_features & 3 == 3
    return memory_bidirectional and sync_bidirectional


def review(run):
    root = Path(__file__).resolve().parents[1]
    original = json.loads((run / 'result.json').read_text(encoding='utf-8-sig'))
    manifest = json.loads((run / 'run-manifest.json').read_text(encoding='utf-8-sig'))
    assert manifest['source_sha256'] == hashlib.sha256((root / 'experiments/dlssg-crossapi-interop/probe.cpp').read_bytes()).hexdigest()
    assert manifest['exe_sha256'] == hashlib.sha256((root / 'experiments/dlssg-crossapi-interop/build/dlssg_crossapi_interop.exe').read_bytes()).hexdigest()
    assert original['same_gpu'] == 'PASS'
    route = any(export_route(original[k + '_externalMemoryFeatures'], original['fence_externalSemaphoreFeatures'])
                for k in ('d3d12_resource', 'd3d12_heap'))
    assert not route and original['if_fail_stage'] == 'CAPABILITY'
    expected = {
        'builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.f0e41bf8.opengl-fg-backpressure.jar': '20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896',
        'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar': 'ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53',
        'wisteria/native/build/fsr-windows-x64/wisteria_fsr_bridge.dll': 'b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b',
    }
    actual = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in expected}
    assert actual == expected
    result = {**original, 'run_id': run.name, 'phase_reached': 'A_CAPABILITY',
              'literal_vulkan_native_export_route': 'BLOCKED_CAPABILITY',
              'all_gpu_interop_routes_infeasible': 'NOT_PROVEN',
              'future_route_candidate': 'D3D12-created allocation/fence imported into Vulkan; not executed',
              'synchronization_query_semantics': 'Default binary semaphore; timeline variant NOT_QUERIED',
              'amd_baseline_modified': False, 'baseline_sha256': actual,
              'presentworker_modified': False, 'wisteria_modified': False,
              'vulkan_dlssg_retested': False, 'dlssg_d3d12_x2_historical': 'PASS',
              'direct_vulkan_dlssg_x2_historical': 'FAIL_AT_KERNEL_CREATION',
              'same_allocation_shared_across_apis': 'NOT_DEMONSTRATED',
              'gpu_copy_required': 'NOT_DETERMINED', 'device_lost': False,
              'validation_limit': 'Khronos layer unavailable; zero observed Vulkan callback errors is not a full validation-layer proof',
              'worker_exit_observation': 'UNKNOWN; wrapper did not capture exit code, no rerun',
              'bridge_timings': 'NOT_RUN; probe wall clock is not handoff/GPU/roundtrip latency',
              'root_cause': 'Vulkan reports IMPORTABLE but not EXPORTABLE for the queried D3D12 memory/fence handles; stop before requested native Vulkan export',
              'dlssg_vulkan_sidecar_prerequisites': 'NOT_READY',
              'original_evidence_sha256': {p: hashlib.sha256((run / p).read_bytes()).hexdigest()
                  for p in ('result.json', 'capabilities.json', 'events.jsonl', 'run-manifest.json')},
              'public_sources': [
                  'https://docs.vulkan.org/refpages/latest/refpages/source/VkExternalMemoryProperties.html',
                  'https://docs.vulkan.org/refpages/latest/refpages/source/VkExternalMemoryHandleTypeFlagBits.html',
                  'https://docs.vulkan.org/refpages/latest/refpages/source/VkExternalSemaphoreProperties.html',
                  'https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createsharedhandle']}
    (run / 'reviewed-result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({'stage': 'CAPABILITY', 'native_export': 'BLOCKED', 'baseline_unchanged': True,
                      'alternative_allocation_origin_not_tested': True}))


if __name__ == '__main__':
    review(Path(sys.argv[1]).resolve())
