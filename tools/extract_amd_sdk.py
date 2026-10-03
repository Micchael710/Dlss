"""Extract only the official 1.1.4 API, Vulkan DLL and notices; never execute it."""
import hashlib
import json
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
destination = root / 'third_party/amd-fidelityfx-1.1.4'
archive = destination / 'downloads/FidelityFX-SDK-v1.1.4.zip'
members = {}
with zipfile.ZipFile(archive) as sdk:
    for name in sdk.namelist():
        selected = (name.startswith('ffx-api/include/')
                    or name.startswith('ffx-api/src/')
                    or name == 'ffx-api/bin/amd_fidelityfx_vk.dll'
                    or ('license' in name.lower() or 'notice' in name.lower())
                    and '/' not in name.strip('/')
                    or name == 'sdk/LICENSE.txt')
        if not selected or name.endswith('/'):
            continue
        path = (destination / name).resolve()
        if not path.is_relative_to(destination.resolve()):
            raise ValueError(name)
        data = sdk.read(name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        members[name] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
manifest = {'origin': 'https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/download/v1.1.4/FidelityFX-SDK-v1.1.4.zip',
            'tag': 'v1.1.4', 'sdk': '1.1.4', 'fsr_target': '3.1.4',
            'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
            'files': members}
(destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps({'archive_sha256': manifest['archive_sha256'],
                  'extracted_files': len(members),
                  'vulkan_dll': members['ffx-api/bin/amd_fidelityfx_vk.dll']}, indent=2))
