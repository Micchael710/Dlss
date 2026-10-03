"""Pin official later tags and download research text, never binaries or mod changes."""
import hashlib, json, urllib.request
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'logs/research'
repo = 'GPUOpen-LibrariesAndSDKs/FidelityFX-SDK'

def fetch(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'FSR-source-audit', 'Accept': 'application/vnd.github+json'})
    with urllib.request.urlopen(request, timeout=45) as response:
        return response.read()

for tag in ['v2.0.0', 'v2.1.0']:
    dest = root / tag
    dest.mkdir(parents=True, exist_ok=True)
    commit = json.loads(fetch(f'https://api.github.com/repos/{repo}/commits/{tag}'))['sha']
    tree_url = f'https://api.github.com/repos/{repo}/git/trees/{commit}?recursive=1'
    tree_data = fetch(tree_url)
    (dest/'official-tree.json').write_bytes(tree_data)
    tree = json.loads(tree_data)
    assert not tree.get('truncated'), 'Cannot infer absent source from incomplete tree'
    records = []
    for entry in tree['tree']:
        name = entry['path']
        low = name.lower()
        if entry['type'] != 'blob': continue
        selected = (low.endswith(('ffx_framegeneration.h','ffx_frameinterpolation.h','ffx_frameinterpolation.cpp','ffx_provider_framegeneration.cpp','frameinterpolationswapchainvk.cpp'))
                    or ('frame-interpolation' in low or 'framegeneration' in low or 'frameinterpolation' in low) and low.endswith(('.md','.h','.cpp','.hlsl','.glsl'))
                    or low.endswith(('version_2_0_0.md','version_2_1_0.md','readme.md')) and (low.count('/') < 2 or 'docs' in low))
        if not selected: continue
        url = f'https://raw.githubusercontent.com/{repo}/{commit}/{name}'
        data = fetch(url)
        target = (dest/name).resolve()
        assert target.is_relative_to(dest.resolve())
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        records.append({'path':name, 'url':url, 'sha256':hashlib.sha256(data).hexdigest(), 'bytes':len(data)})
    manifest = {'tag':tag,'commit':commit,'tree_url':tree_url,'research_only':True,'files':records}
    (dest/'audit-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps({'tag':tag,'commit':commit,'files':len(records),'paths':[r['path'] for r in records]},indent=2),flush=True)
