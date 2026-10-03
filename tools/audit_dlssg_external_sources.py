"""Pinned text-only evidence for the external-loader audit; never fetch DLLs."""
import concurrent.futures
import hashlib
import json
import sys
from pathlib import Path
import urllib.request
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1] / 'logs/research/dlssg-external'
REPOS = ['ShyVortex/dlss-unlocked', 'ShyVortex/OptiScaler-DLSSNR-PreSR-Multipass',
         'optiscaler/OptiScaler']
EXT = {'.md', '.txt', '.h', '.hpp', '.cpp', '.c', '.ini', '.json', '.iss', '.ps1', '.yml', '.yaml'}

def get(url):
    req = urllib.request.Request(url, headers={'User-Agent': 'Wisteria-source-audit'})
    with urllib.request.urlopen(req, timeout=40) as response:
        return response.read()

def selected(path):
    p = path.lower()
    return (Path(p).suffix in EXT or Path(p).name in {'license', 'copying'}) and not any(
        x in p for x in ['external/', 'third_party/', 'vendor/', 'imgui/', 'spdlog/', 'reshade/'])

def audit(repo):
    base = ROOT / repo.replace('/', '__')
    if (base / 'manifest.json').exists():
        print(repo, 'already pinned', flush=True)
        return
    if (base / 'tree.json').exists():
        tree = json.loads((base / 'tree.json').read_text(encoding='utf-8'))
        sha = tree['sha']
    else:
        info = json.loads(get(f'https://api.github.com/repos/{repo}'))
        sha = json.loads(get(f'https://api.github.com/repos/{repo}/commits/{info["default_branch"]}'))['sha']
        tree = json.loads(get(f'https://api.github.com/repos/{repo}/git/trees/{sha}?recursive=1'))
    assert not tree.get('truncated')
    base.mkdir(parents=True, exist_ok=True)
    (base / 'tree.json').write_text(json.dumps(tree, indent=2), encoding='utf-8')
    items = [i for i in tree['tree'] if i['type'] == 'blob' and selected(i['path']) and i.get('size', 0) < 1000000]
    def fetch(i):
        url = f'https://raw.githubusercontent.com/{repo}/{sha}/{quote(i["path"])}'
        target = base / i['path']
        data = target.read_bytes() if target.exists() else get(url)
        try:
            data.decode('utf-8-sig')
        except UnicodeDecodeError:
            data.decode('cp1252')
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        assert blob == i['sha']
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        return {'path': i['path'], 'url': url, 'sha256': hashlib.sha256(data).hexdigest(), 'git_blob': blob}
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        files = list(pool.map(fetch, items))
    (base / 'manifest.json').write_text(json.dumps({'repo': repo, 'commit': sha,
        'files': files, 'binary_downloads': 0, 'tree_truncated': False}, indent=2), encoding='utf-8')
    print(repo, sha, len(files), 'verified text files', flush=True)

if __name__ == '__main__':
    if sys.argv[1:] == ['--verify']:
        count = 0
        for repo in REPOS:
            base = ROOT / repo.replace('/', '__')
            manifest = json.loads((base / 'manifest.json').read_text(encoding='utf-8'))
            assert manifest['binary_downloads'] == 0
            for item in manifest['files']:
                data = (base / item['path']).read_bytes()
                assert hashlib.sha256(data).hexdigest() == item['sha256'], item['path']
                assert hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == item['git_blob']
                count += 1
        print(json.dumps({'verified_text_files': count, 'binary_downloads': 0, 'network_used': False}))
    else:
        assert not sys.argv[1:]
        for repo in REPOS:
            audit(repo)
