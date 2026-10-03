"""Fetch and index pinned public MFG documentation. Never fetch or load DLLs."""
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'logs/research/dlssg-mfg'
REPO = 'NVIDIA/DLSS'
COMMIT = '374959484e79a640feaba44c93ac8cfb0a03f5b5'
PATH = 'doc/DLSS-FG Programming Guide.pdf'

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sl_commit = '2122257e0fce486f91b385aa63b9a09b0a34b363'
    records = []
    sources = [
        ('Streamline-changelog.txt', f'https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/{sl_commit}/changelog.txt'),
        ('Streamline-2.11.1-release.json', 'https://api.github.com/repos/NVIDIA-RTX/Streamline/releases/tags/v2.11.1'),
        ('SM86-issue-518.json', 'https://api.github.com/repos/sdli1995/dlssg_for_sm86/issues/518'),
        ('SM86-discussion-546.html', 'https://github.com/sdli1995/dlssg_for_sm86/discussions/546'),
    ]
    for name, source_url in sources:
        source_path = OUT / name
        if not source_path.exists():
            req = urllib.request.Request(source_url, headers={'User-Agent': 'Wisteria-MFG-documentation-audit'})
            with urllib.request.urlopen(req, timeout=40) as response:
                source_data = response.read()
            source_data.decode('utf-8')
            source_path.write_bytes(source_data)
        source_data = source_path.read_bytes()
        if name == 'Streamline-changelog.txt':
            sl_tree = json.loads((ROOT / 'logs/research/dlssg-ampere/NVIDIA-RTX__Streamline/tree.json').read_text())
            entry = next(x for x in sl_tree['tree'] if x['path'] == 'changelog.txt')
            assert hashlib.sha1(f'blob {len(source_data)}\0'.encode() + source_data).hexdigest() == entry['sha']
        records.append({'path': name, 'url': source_url, 'sha256': hashlib.sha256(source_data).hexdigest()})
    (OUT / 'supplemental-manifest.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
    target = OUT / 'NVIDIA-DLSS-FG-Programming-Guide.pdf'
    url = f'https://raw.githubusercontent.com/{REPO}/{COMMIT}/doc/DLSS-FG%20Programming%20Guide.pdf'
    if not target.exists():
        req = urllib.request.Request(url, headers={'User-Agent': 'Wisteria-MFG-documentation-audit'})
        with urllib.request.urlopen(req, timeout=55) as response:
            data = response.read()
        tree = json.loads((ROOT / 'logs/research/dlssg-ampere/NVIDIA__DLSS/tree.json').read_text())
        entry = next(x for x in tree['tree'] if x['path'] == PATH)
        assert hashlib.sha1(f'blob {len(data)}\0'.encode() + data).hexdigest() == entry['sha']
        assert data.startswith(b'%PDF-')
        target.write_bytes(data)
    data = target.read_bytes()
    from pypdf import PdfReader
    reader = PdfReader(target)
    text = []
    hits = []
    needles = ('multiframe', 'multi frame', 'multi-frame', 'framecount', 'frameindex',
               'history', 'frame pair', 'getcapability', 'getdevicecapability')
    for index, page in enumerate(reader.pages, 1):
        body = page.extract_text() or ''
        text.append(f'\n--- PDF PAGE {index} ---\n{body}')
        if any(term in body.lower() for term in needles):
            hits.append(index)
    (OUT / 'NVIDIA-DLSS-FG-Programming-Guide.txt').write_text('\n'.join(text), encoding='utf-8')
    record = {'url': url, 'commit': COMMIT, 'sha256': hashlib.sha256(data).hexdigest(),
              'bytes': len(data), 'pages': len(reader.pages), 'matched_pages': hits,
              'dll_downloads': 0, 'vendor_code_executed': False}
    (OUT / 'pdf-manifest.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
    evidence = []
    for name in ['nvsdk_ngx_params_dlssg.h', 'nvsdk_ngx_defs_dlssg.h',
                 'nvsdk_ngx_helpers_dlssg_vk.h', 'nvsdk_ngx_helpers_dlssg_d3d.h']:
        source = ROOT / 'logs/research/dlssg-ampere/NVIDIA__DLSS/include' / name
        body = source.read_bytes()
        lines = body.decode('utf-8').splitlines()
        evidence.append({'path': str(source.relative_to(ROOT)),
                         'commit': COMMIT, 'sha256': hashlib.sha256(body).hexdigest(),
                         'matches': [{'line': n, 'text': line.strip()}
                                     for n, line in enumerate(lines, 1)
                                     if any(term in line for term in ['ultiFrame', 'EvaluateFeature_C'])]})
    (OUT / 'header-evidence.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    print(json.dumps(record))

if __name__ == '__main__':
    main()
