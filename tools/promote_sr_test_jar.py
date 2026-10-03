"""Verify and retain a uniquely labelled SR test JAR; select it in isolated launcher."""
import hashlib
import json
import re
import shutil
import struct
import sys
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).resolve()
label = sys.argv[2]
assert re.fullmatch(r"[a-z0-9-]+", label)
assert source.parent == (root / "superresolution/neoforge/build/libs").resolve()
with zipfile.ZipFile(source) as jar:
    assert jar.testzip() is None
    native = jar.read("lib/libSuperResolution+win64+release.dll")
    assert hashlib.sha256(native).hexdigest() == "80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec"
    own = [p for p in jar.namelist() if p.startswith("io/homo/superresolution/") and p.endswith(".class")]
    assert own
    assert all(struct.unpack_from(">H", jar.read(p), 6)[0] == 65 for p in own)
destination = root / "builds/baseline" / (source.stem + "-" + label + ".jar")
digest = hashlib.sha256(source.read_bytes()).hexdigest()
if destination.exists():
    assert hashlib.sha256(destination.read_bytes()).hexdigest() == digest, "Refusing to overwrite a different artifact"
else:
    shutil.copy2(source, destination)
launcher = root / "runtime-baseline/build.gradle"
text = launcher.read_text(encoding="utf-8")
text, count = re.subn(r"runtimeOnly files\('\.\./builds/baseline/super_resolution[^']+\.jar'\)",
                    f"runtimeOnly files('../builds/baseline/{destination.name}')", text)
assert count == 1
launcher.write_text(text, encoding="utf-8")
result = {"file": destination.name, "sha256": digest, "bytes": destination.stat().st_size,
          "java21_own_classes": len(own), "native_core_unchanged": True, "label": label}
(root / f"logs/runtime/sr-{label}-artifact.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result, indent=2))
