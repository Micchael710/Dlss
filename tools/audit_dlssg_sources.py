"""Save pinned public text sources for the DLSS-G research. No binary downloads."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import urllib.request
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1] / 'logs/research/dlssg-ampere'
REPOS = ['sdli1995/dlssg_for_sm86', 'NVIDIA-RTX/Streamline',
         'Nukem9/dlssg-to-fsr3', 'optiscaler/OptiScaler', 'NVIDIA/DLSS',
         'pipotoufikxyz-lgtm/dlssg_for_sm86-MFG-version', 'thierbig/bg3fgvk']
EXT = {'.md', '.txt', '.h', '.hpp', '.cpp', '.c', '.ini', '.comp', '.json'}

def get(url):
    req = urllib.request.Request(url, headers={'User-Agent': 'Minecraft-DLSS-research'})
    with urllib.request.urlopen(req, timeout=40) as response:
        return response.read()

def selected(repo, path):
    p = path.lower()
    if Path(path).suffix.lower() not in EXT and Path(path).name not in {'LICENSE', 'COPYING'}:
        return False
    if repo.startswith('sdli1995/'):
        return 'evidence/' not in p
    if repo.startswith('pipotoufikxyz-lgtm/'):
        return not p.startswith('.github/')
    if repo == 'thierbig/bg3fgvk':
        return any(s in p for s in ['readme', 'license', 'streamline', 'vulkan', 'dlss', 'slboot', 'vkhooks', 'inputs.cpp']) and 'external/' not in p
    if any(s in p for s in ['license', 'copying', 'notice']):
        return True
    if p.endswith('readme.md') and '/' not in path:
        return True
    if repo == 'NVIDIA/DLSS':
        return any(s in p for s in ['nvsdk_ngx_helpers_vk', 'nvsdk_ngx_vk.h', 'nvsdk_ngx_defs.h', 'nvsdk_ngx_helpers_dlssg', 'nvsdk_ngx_defs_dlssg', 'nvsdk_ngx_params_dlssg'])
    if repo == 'NVIDIA-RTX/Streamline':
        return any(s in p for s in ['programmingguidedlss_g', 'sl_dlss_g.h', 'sl_helpers_vk', 'sl_reflex.h', 'sl_core_types.h'])
    if repo.startswith('Nukem9/'):
        return any(s in p for s in ['vulkan', '_vk', 'dlss', 'ngx', 'ffframeinterpolatorvk']) and 'external/' not in p and 'dependencies/' not in p
    return any(s in p for s in ['dlssg', 'dlss_g', 'ngx', 'vkon12', 'vkwdx12', 'vulkanwdx12', 'spoofing.md', 'features.md', 'optiscaler.ini']) and 'external/' not in p

def audit(repo):
    base = ROOT / repo.replace('/', '__')
    if (base / 'manifest.json').exists():
        manifest = json.loads((base / 'manifest.json').read_text(encoding='utf-8'))
        tree = json.loads((base / 'tree.json').read_text(encoding='utf-8'))
        commit = manifest['commit']
        known = {item['path'] for item in manifest['files']}
        additional = [item['path'] for item in tree['tree'] if item['type'] == 'blob' and selected(repo, item['path']) and item['path'] not in known and item.get('size', 0) < 1000000]
        for path in additional:
            data = get(f'https://raw.githubusercontent.com/{repo}/{commit}/{quote(path)}')
            target = base / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            manifest['files'].append({'path': path, 'url': f'https://raw.githubusercontent.com/{repo}/{commit}/{quote(path)}', 'sha256': hashlib.sha256(data).hexdigest()})
        (base / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        print(repo, 'already saved', flush=True)
        return
    info = json.loads(get(f'https://api.github.com/repos/{repo}'))
    commit = json.loads(get(f'https://api.github.com/repos/{repo}/commits/{info["default_branch"]}'))['sha']
    tree = json.loads(get(f'https://api.github.com/repos/{repo}/git/trees/{commit}?recursive=1'))
    base = ROOT / repo.replace('/', '__')
    base.mkdir(parents=True, exist_ok=True)
    (base / 'tree.json').write_text(json.dumps(tree, indent=2), encoding='utf-8')
    paths = [item['path'] for item in tree['tree'] if item['type'] == 'blob' and selected(repo, item['path']) and item.get('size', 0) < 1000000]
    files = []
    def fetch(path):
        url = f'https://raw.githubusercontent.com/{repo}/{commit}/{quote(path)}'
        data = get(url)
        try:
            data.decode('utf-8')
        except UnicodeDecodeError:
            data.decode('cp1252')
        target = base / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        return {'path': path, 'url': url, 'sha256': hashlib.sha256(data).hexdigest()}
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        for item in pool.map(fetch, paths):
            files.append(item)
    manifest = {'repo': repo, 'commit': commit, 'tree_truncated': tree.get('truncated'), 'files': files, 'binary_downloads': 0}
    (base / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(repo, commit, len(files), 'text files', flush=True)

if __name__ == '__main__':
    for repo in REPOS:
        audit(repo)
