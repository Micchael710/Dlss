"""Read-only inspection of baseline artifacts and research-source integrity."""
import hashlib
import json
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
research = root / 'logs/research/dlssg-ampere'
expected = {
    'builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.f0e41bf8.opengl-fg-backpressure.jar': '20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896',
    'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-fsr-backpressure.jar': 'ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53',
    'wisteria/native/build/fsr-windows-x64/wisteria_fsr_bridge.dll': 'b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b',
}
artifacts = []
for relative, digest in expected.items():
    path = root / relative
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    assert actual == digest, relative
    record = {'path': relative, 'sha256': actual, 'baseline_match': True}
    if path.suffix == '.jar':
        with zipfile.ZipFile(path) as jar:
            record['native_entries'] = [p for p in jar.namelist() if p.lower().endswith(('.dll', '.so', '.dylib'))]
            record['ngx_jni_present'] = any('libSuperResolutionNGX' in p for p in jar.namelist())
    artifacts.append(record)
sources = []
for path in research.glob('*/manifest.json'):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    assert manifest['binary_downloads'] == 0
    for file in manifest['files']:
        assert hashlib.sha256((path.parent / file['path']).read_bytes()).hexdigest() == file['sha256']
    tree = json.loads((path.parent / 'tree.json').read_text(encoding='utf-8'))
    sources.append({'repo': manifest['repo'], 'commit': manifest['commit'],
                    'verified_text_files': len(manifest['files']),
                    'source_entries': [p['path'] for p in tree['tree'] if p['type'] == 'blob' and Path(p['path']).suffix.lower() in {'.c', '.cpp', '.h', '.hpp'}],
                    'tree_truncated': tree.get('truncated')})
result = {'date': '2026-10-03', 'scope': 'static research, no runtime execution or compilation',
          'artifacts': artifacts, 'sources': sources,
          'ngx_sdk_headers_present': (root / 'superresolution/native/cpp/SRNativeNGX/third_party/DLSS/include/nvsdk_ngx_vk.h').exists(),
          'aborted_temporal_interpolator_present': (root / 'wisteria/common/src/main/java/org/ireallywanttosleep/wisteria/temporal/TemporalInterpolator.java').exists(),
          'local_git_metadata_present': {p: (root / p / '.git').exists() for p in ['superresolution', 'wisteria']}}
(research / 'local-evidence.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({'baseline_hashes_match': True, 'repos': len(sources),
                  'verified_text_files': sum(p['verified_text_files'] for p in sources),
                  'sr_ngx_jni_present': artifacts[0]['ngx_jni_present'],
                  'ngx_sdk_headers_present': result['ngx_sdk_headers_present'],
                  'temporal_partial_patch_present': result['aborted_temporal_interpolator_present']}, indent=2))
