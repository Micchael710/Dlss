"""Stage pinned official NVIDIA stock assets only; never download community DLLs."""
import hashlib
import json
from pathlib import Path
import urllib.request
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '374959484e79a640feaba44c93ac8cfb0a03f5b5'
OUT = ROOT / 'experiments/ngx-dlssg-probe/vendor'
TREE = json.loads((ROOT / 'logs/research/dlssg-ampere/NVIDIA__DLSS/tree.json').read_text())
ASSETS = ['lib/Windows_x86_64/x64/nvsdk_ngx_s.lib',
          'lib/Windows_x86_64/rel/nvngx_dlssg.dll', 'LICENSE.txt']

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = []
    for path in ASSETS:
        target = OUT / Path(path).name
        url = f'https://raw.githubusercontent.com/NVIDIA/DLSS/{COMMIT}/{quote(path)}'
        if not target.exists():
            req = urllib.request.Request(url, headers={'User-Agent': 'Wisteria-NGX-stock-capability-probe'})
            with urllib.request.urlopen(req, timeout=45) as response:
                data = response.read()
            entry = next(x for x in TREE['tree'] if x['path'] == path)
            assert hashlib.sha1(f'blob {len(data)}\0'.encode() + data).hexdigest() == entry['sha']
            target.write_bytes(data)
        data = target.read_bytes()
        entry = next(x for x in TREE['tree'] if x['path'] == path)
        assert hashlib.sha1(f'blob {len(data)}\0'.encode() + data).hexdigest() == entry['sha']
        manifest.append({'path': path, 'url': url, 'commit': COMMIT,
                         'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    main()
