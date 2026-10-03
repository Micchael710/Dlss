"""Read exact pinned SDK archive; retain research text only, never modify runtime/source mods."""
import hashlib, json, zipfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
sdk = root/'third_party/amd-fidelityfx-1.1.4'
archive = sdk/'downloads/FidelityFX-SDK-v1.1.4.zip'
expected = json.loads((sdk/'manifest.json').read_text(encoding='utf-8'))['archive_sha256']
assert hashlib.sha256(archive.read_bytes()).hexdigest() == expected
dest = root/'logs/research/fsr-1.1.4'
records = []
with zipfile.ZipFile(archive) as zipped:
    for name in zipped.namelist():
        low = name.lower()
        selected = (name in ['ffx-api/include/ffx_api/ffx_framegeneration.h', 'ffx-api/src/ffx_provider_framegeneration.cpp']
                    or low.endswith(('ffx_frameinterpolation.h','ffx_frameinterpolation.cpp','frameinterpolationswapchainvk.cpp','ffx_fsr3.cpp','ffx_fsr3.h'))
                    or '/gpu/frameinterpolation/' in low and low.endswith(('.h','.hlsl','.glsl'))
                    or low.endswith('ffx_frameinterpolation_private.h')
                    or (low.startswith('docs/') or '/docs/' in low) and low.endswith('.md') and ('frame-interpolation' in low or 'fsr3' in low or 'fsr3' in Path(low).name))
        if not selected or name.endswith('/'): continue
        target = (dest/name).resolve()
        assert target.is_relative_to(dest.resolve())
        data = zipped.read(name)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        records.append(dict(path=name, sha256=hashlib.sha256(data).hexdigest(), bytes=len(data)))
manifest = dict(sdk='1.1.4', archive_sha256=expected, research_only=True, files=records)
(dest/'audit-manifest.json').write_text(json.dumps(manifest,indent=2), encoding='utf-8')
print(json.dumps({'file_count':len(records), 'host_docs':[r['path'] for r in records if '/gpu/' not in r['path']]},indent=2))
