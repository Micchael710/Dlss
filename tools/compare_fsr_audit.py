"""Offline, reproducible source comparison of the three pinned research snapshots."""
import difflib, hashlib, json
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'logs/research'
for version in ['fsr-1.1.4','v2.0.0','v2.1.0']:
    manifest = json.loads((root/version/'audit-manifest.json').read_text(encoding='utf-8'))
    for item in manifest['files']:
        assert hashlib.sha256((root/version/item['path']).read_bytes()).hexdigest() == item['sha256'], item['path']

mapping = {
 'public-api': ('ffx-api/include/ffx_api/ffx_framegeneration.h','Kits/FidelityFX/framegeneration/include/ffx_framegeneration.h'),
 'provider': ('ffx-api/src/ffx_provider_framegeneration.cpp','Kits/FidelityFX/framegeneration/fsr3/internal/ffx_provider_fsr3framegeneration.cpp'),
 'dispatch-api': ('sdk/include/FidelityFX/host/ffx_frameinterpolation.h','Kits/FidelityFX/framegeneration/fsr3/include/ffx_frameinterpolation.h'),
 'dispatch-implementation': ('sdk/src/components/frameinterpolation/ffx_frameinterpolation.cpp','Kits/FidelityFX/framegeneration/fsr3/internal/ffx_frameinterpolation.cpp'),
}
for name in ['ffx_frameinterpolation.h','ffx_frameinterpolation_callbacks_hlsl.h','ffx_frameinterpolation_game_motion_vector_field.h','ffx_frameinterpolation_optical_flow_vector_field.h','ffx_frameinterpolation_reconstruct_previous_depth.h']:
    mapping['shader-'+name] = ('sdk/include/FidelityFX/gpu/frameinterpolation/'+name,'Kits/FidelityFX/framegeneration/fsr3/include/gpu/frameinterpolation/'+name)

dest = root/'comparisons'
dest.mkdir(exist_ok=True)
summary=[]
for label,(old_path,new_path) in mapping.items():
    texts = [(root/'fsr-1.1.4'/old_path).read_text(encoding='utf-8-sig').splitlines(keepends=True)]
    texts += [(root/tag/new_path).read_text(encoding='utf-8-sig').splitlines(keepends=True) for tag in ['v2.0.0','v2.1.0']]
    for a,b in [(0,1),(1,2)]:
        tags=['fsr-1.1.4','v2.0.0','v2.1.0']
        diff=''.join(difflib.unified_diff(texts[a],texts[b],fromfile=tags[a],tofile=tags[b]))
        filename=f'{label}-{tags[a]}-to-{tags[b]}.diff'
        (dest/filename).write_text(diff,encoding='utf-8')
        summary.append({'file':filename,'identical':not diff,'diff_lines':len(diff.splitlines())})
inventory=[]
for tag in ['v2.0.0','v2.1.0']:
    tree=json.loads((root/tag/'official-tree.json').read_text())
    assert not tree['truncated']
    paths=[e['path'] for e in tree['tree'] if e['type']=='blob']
    vk=[p for p in paths if 'framegeneration' in p.lower() and ('vulkan' in p.lower() or '/vk/' in p.lower() or p.lower().endswith('swapchainvk.cpp'))]
    inventory.append({'tag':tag,'tree_complete':True,'framegeneration_vulkan_paths':vk})
result={'all_downloaded_hashes_verified':True,'comparisons':summary,'later_vulkan_inventory':inventory}
(dest/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
