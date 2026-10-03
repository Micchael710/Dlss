"""Inspect public distribution metadata, PE tables and hashes; never load DLLs.

No disassembly, kernel extraction, binary patching or invocation of exports.
"""
import hashlib
import json
import struct
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'logs/research/dlssg-ampere/deep'
COMMIT = '9621db573e07ed54f50c15bbb585ed9a7bdfac28'

def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'Wisteria-FG-ABI-audit'}), timeout=50) as response:
        return response.read()

def pe_metadata(data):
    if data[:2] != b'MZ':
        return None
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0':
        return None
    machine, count = struct.unpack_from('<HH', data, pe+4)
    optsize = struct.unpack_from('<H', data, pe+20)[0]
    opt = pe+24
    magic = struct.unpack_from('<H', data, opt)[0]
    dirs = opt+(112 if magic == 0x20b else 96)
    sections = []
    for i in range(count):
        at = opt+optsize+i*40
        vsize, rva, rawsize, raw = struct.unpack_from('<IIII', data, at+8)
        sections.append((rva, max(vsize, rawsize), raw))
    def offset(rva):
        for base, size, raw in sections:
            if base <= rva < base+size:
                return raw+rva-base
        if rva < len(data):
            return rva
        raise ValueError('unmapped PE RVA')
    def cstr(rva):
        at = offset(rva)
        return data[at:data.index(b'\0', at)].decode('ascii', errors='replace')
    exports, imports, resources = [], [], []
    er, _ = struct.unpack_from('<II', data, dirs)
    if er:
        at = offset(er)
        n = struct.unpack_from('<I', data, at+24)[0]
        names = offset(struct.unpack_from('<I', data, at+32)[0])
        exports = [cstr(struct.unpack_from('<I', data, names+4*i)[0]) for i in range(n)]
    ir, _ = struct.unpack_from('<II', data, dirs+8)
    if ir:
        at = offset(ir)
        while any(data[at:at+20]):
            imports.append(cstr(struct.unpack_from('<I', data, at+12)[0]))
            at += 20
    rr, _ = struct.unpack_from('<II', data, dirs+16)
    if rr:
        resource_base = offset(rr)
        def walk(rel, path):
            at = resource_base+rel
            named, ids = struct.unpack_from('<HH', data, at+12)
            for i in range(named+ids):
                key, child = struct.unpack_from('<II', data, at+16+8*i)
                if key & 0x80000000:
                    loc = resource_base+(key & 0x7fffffff)
                    length = struct.unpack_from('<H', data, loc)[0]
                    label = data[loc+2:loc+2+2*length].decode('utf-16le')
                else:
                    label = str(key)
                route = path+[label]
                if child & 0x80000000:
                    walk(child & 0x7fffffff, route)
                else:
                    rva, size = struct.unpack_from('<II', data, resource_base+child)
                    payload = data[offset(rva):offset(rva)+size]
                    record = {'resource_path': route, 'bytes': size, 'sha256': hashlib.sha256(payload).hexdigest()}
                    if payload[:2] == b'MZ':
                        record['embedded_pe'] = pe_metadata(payload)
                    resources.append(record)
        walk(0, [])
    return {'machine': hex(machine), 'exports': exports, 'imports': imports, 'resources': resources}

if __name__ == '__main__':
    ROOT.mkdir(parents=True, exist_ok=True)
    metadata = {}
    for name, endpoint in [('branches', 'branches'), ('tags', 'tags'), ('releases', 'releases'), ('source_issue', 'issues/17'), ('source_issue_comments', 'issues/17/comments'), ('vulkan_issue_comments', 'issues/5/comments'), ('source_request_comments', 'issues/524/comments')]:
        url = f'https://api.github.com/repos/sdli1995/dlssg_for_sm86/{endpoint}'
        try:
            value = json.loads(get(url))
            (ROOT / f'{name}.json').write_text(json.dumps(value, indent=2), encoding='utf-8')
            metadata[name] = {'url': url, 'entries': len(value) if isinstance(value, list) else 1}
        except Exception as error:
            metadata[name] = {'url': url, 'error': str(error)}
    sdk = ROOT / 'official-sdk'
    sdk_commit = '374959484e79a640feaba44c93ac8cfb0a03f5b5'
    sdk_tree = json.loads((ROOT.parent / 'NVIDIA__DLSS/tree.json').read_text(encoding='utf-8'))
    sdk_records = []
    for item in sdk_tree['tree']:
        path = item['path']
        if item['type'] != 'blob' or not (path.startswith('include/') or path == 'LICENSE.txt'):
            continue
        target = sdk / path
        target.parent.mkdir(parents=True, exist_ok=True)
        url = f'https://raw.githubusercontent.com/NVIDIA/DLSS/{sdk_commit}/{path}'
        if not target.exists():
            target.write_bytes(get(url))
        sdk_records.append({'path': path, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest(), 'url': url})
    (sdk / 'manifest.json').write_text(json.dumps({'commit': sdk_commit, 'files': sdk_records}, indent=2), encoding='utf-8')
    url = f'https://raw.githubusercontent.com/sdli1995/dlssg_for_sm86/{COMMIT}/version.dll'
    path = ROOT / 'components/sdli-0.3.5/version.dll'
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        payload = get(url)
        if payload[:2] != b'MZ':
            raise ValueError('Expected an original PE distribution artifact')
        path.write_bytes(payload)
    payload = path.read_bytes()
    expected_git_blob = next(item['sha'] for item in json.loads((ROOT.parent / 'sdli1995__dlssg_for_sm86/tree.json').read_text(encoding='utf-8'))['tree'] if item['path'] == 'version.dll')
    blob = hashlib.sha1(f'blob {len(payload)}\0'.encode()+payload).hexdigest()
    assert blob == expected_git_blob, 'Downloaded bytes differ from the pinned Git tree'
    report = {'repo': 'sdli1995/dlssg_for_sm86', 'commit': COMMIT, 'documented_release': '0.3.5', 'url': url,
              'path': str(path), 'bytes': len(payload), 'sha256': hashlib.sha256(payload).hexdigest(),
              'git_blob_verified': blob, 'dll_executed': False, 'installed': False,
              'metadata_queries': metadata, 'pe': pe_metadata(payload)}
    (ROOT / 'sm86-component.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'sha256': report['sha256'], 'bytes': len(payload), 'exports': report['pe']['exports'],
                      'resources': [(p['resource_path'], p['bytes'], bool(p.get('embedded_pe'))) for p in report['pe']['resources']]}, indent=2))
