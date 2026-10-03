"""Validate a real capability measurement; do not invoke a GPU/runtime."""
import hashlib
import json
import sys
from pathlib import Path

directory = Path(sys.argv[1]).resolve()
raw = json.loads((directory / 'raw-result.json').read_text(encoding='utf-8-sig'))
identity = json.loads((directory / 'runtime-identity.json').read_text(encoding='utf-8-sig'))
assert raw['scenario'] == identity['scenario'] == 'stock'
assert raw['evaluate_called'] is False and raw['generated_frames'] == 0
assert raw['ngx_init'] == raw['capability_query'] == '0x00000001'
assert raw['gpu'] == 'NVIDIA GeForce RTX 3050 Ti Laptop GPU'
assert raw['available']['result'] == '0x00000001'
assert hashlib.sha256(Path(identity['runtime_path']).read_bytes()).hexdigest() == identity['runtime_sha256']
assert identity['runtime_path'] in raw['ngx_modules']
if raw['available']['value'] != 1:
    assert raw['create_feature'] == 'SKIPPED_FG_UNAVAILABLE'
for field in ['available', 'feature_init_result', 'needs_updated_driver', 'multi_frame_count_max']:
    if raw[field]['result'] != '0x00000001':
        assert raw[field]['value'] is None, 'Failed getter must not become a fabricated zero/default'
identity['sm86_adapted_probe'] = 'NOT_RUN: reproducible Vulkan backend unavailable'
result = {
    'stock': {**identity, **raw},
    'sm86': {'status': 'NOT_RUN', 'available': None, 'multi_frame_count_max': None,
             'original_ngx_available_before_hook': None, 'original_ngx_max_before_hook': None,
             'reported_available_after_hook': None, 'reported_max_after_hook': None,
             'reason': 'Versioned installation ABI and reproducible SM86 Vulkan adapter unavailable'},
    'comparison': {'original_available': raw['available']['value'], 'adapted_available': None,
                   'original_max': raw['multi_frame_count_max']['value'], 'adapted_max': None,
                   'original_max_getter': raw['multi_frame_count_max']['result'],
                   'reported_after_hook': None, 'community_hook_installed': False},
    'multipliers_validated': [],
    'baseline_amd_modified': False,
}
result['stock']['feature_init_result']['value_hex'] = f"0x{raw['feature_init_result']['value']:08x}"
(directory / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({'measurement_validated': True, 'stock_available': raw['available'],
                  'maximum': raw['multi_frame_count_max'], 'sm86': 'NOT_RUN',
                  'create_feature': raw['create_feature']}, indent=2))
