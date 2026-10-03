"""Record the completed isolated run; no GPU execution or historical log mutation."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
run = root / 'logs/runtime/dlssg-external-harness/20261003-222208-087'
r = json.loads((run / 'reviewed-result.json').read_text(encoding='utf-8'))
assert r['dlssg_sm86_d3d12_x2'] == 'PASS' and all(r['pass_conditions'].values())
assert r['coordinator']['attempts'] == 1
expected = {
    'builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.f0e41bf8.opengl-fg-backpressure.jar': '20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896',
    'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar': 'ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53',
    'wisteria/native/build/fsr-windows-x64/wisteria_fsr_bridge.dll': 'b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b',
}
actual = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in expected}
assert actual == expected
events = [json.loads(line) for line in (run / 'events.jsonl').read_text().splitlines()]
evaluations = [e for e in events if e['type'] == 'evaluate_begin']
assert len(evaluations) == 5 and all('count=1 index=1' in e['detail'] for e in evaluations)
assert 'realFrameId=4' in evaluations[-1]['detail'] and 'reset=false' in evaluations[-1]['detail']
assert not any('vulkan' in p.lower() for p in r['modules_final'])
named = [json.loads(e['raw']) for e in r['kernel_evidence']]
assert named and all(e['status'] == 0 for e in named)
assert any(e['entry'] == 'Kernel_BlendCandidatesFused' for e in named)
evidence = {
    'run_id': run.name, 'offscreen_x2': 'PASS', 'gpu_runs_this_phase': 1,
    'warmup_evaluations': 4, 'target_evaluations': 1, 'count': 1, 'index': 1,
    'named_kernel_creations_observed': len(named),
    'empty_kernel_diagnostics_not_generation_proof': len(r['diagnostic_kernel_evidence']),
    'backend_local_feature_id': 1, 'ngx_public_feature_enum_separate_namespace': True,
    'baseline_sha256': actual, 'baseline_unchanged': True, 'vulkan_retested': False,
    'vulkan_runtime_loaded': False, 'minecraft_launched': False,
    'bridge_implemented': False, 'mfg_above_x2_tested': False,
    'readback_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in run.glob('*.bin')},
    'evaluate_target_cpu_ms': r['evaluate_cpu_ms'], 'fg_gpu_time_ms': None,
    'timing_limit': 'CPU timing includes host overhead; no GPU timestamps or presentation FPS measured',
}
(run / 'validation-evidence.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
print(json.dumps({'x2': 'PASS', 'baseline_unchanged': True, 'named_kernels': len(named),
                  'diagnostics_separate': len(r['diagnostic_kernel_evidence'])}))
