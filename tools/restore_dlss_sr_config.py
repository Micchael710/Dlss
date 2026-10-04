"""Restore the isolated config byte-for-byte after the controlled runs are closed."""
from pathlib import Path
import sys,json,hashlib
root=Path(__file__).resolve().parents[1]
run=Path(sys.argv[1]).resolve();assert run.is_relative_to(root/'logs/runtime/dlss-sr')
manifest=json.loads((run/'manifest.json').read_text())
original=(run/'original-config.txt').read_bytes()
sha=lambda b:hashlib.sha256(b).hexdigest()
assert sha(original)==manifest['original_config_sha256']
config=root/'runtime-baseline/instance/config/super_resolution/config.toml'
before=config.read_bytes();(run/'config-before-restoration.txt').write_bytes(before)
config.write_bytes(original)
result={'status':'PASS','path':str(config.relative_to(root)),'snapshot_run':run.name,
        'before_sha256':sha(before),'snapshot_sha256':sha(original),'restored_sha256':sha(config.read_bytes()),
        'real_minecraft_touched':False}
assert result['restored_sha256']==result['snapshot_sha256']
(run/'config-restoration.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
