"""Verify the rebuilt JAR and retain the previous baseline unchanged."""
import hashlib
import json
import shutil
import struct
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
name = "super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.e365f5b3.opengl.jar"
old = root / "builds/baseline" / name
assert hashlib.sha256(old.read_bytes()).hexdigest() == "14bedcfe2de3936e3485e441453896fc1f55b55cc222043947a9aa72d8e0b882"
source = root / "superresolution/neoforge/build/libs" / name
destination = old.with_name(name[:-4] + "-native-core.jar")
assert not destination.exists(), "Refusing to overwrite an existing baseline"
native_path = "lib/libSuperResolution+win64+release.dll"
with zipfile.ZipFile(source) as jar, zipfile.ZipFile(old) as previous:
    assert jar.testzip() is None
    embedded = jar.read(native_path)
    native_hash = hashlib.sha256(embedded).hexdigest()
    assert embedded == (root / "superresolution/native/cpp/output/bin/libSuperResolution+win64+release.dll").read_bytes()
    own_classes = [p for p in jar.namelist() if p.startswith("io/homo/superresolution/") and p.endswith(".class")]
    assert own_classes
    for entry in own_classes:
        bytecode = jar.read(entry)
        assert struct.unpack_from(">H", bytecode, 6)[0] == 65, entry
        assert bytecode == previous.read(entry), f"Unexpected Java code change: {entry}"
    natives = [p for p in jar.namelist() if p.lower().endswith((".dll", ".so", ".dylib"))]
    assert natives == [native_path], natives
shutil.copy2(source, destination)
result = {"file": destination.name, "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
          "bytes": destination.stat().st_size, "embedded_native": native_path, "native_sha256": native_hash,
          "java21_own_classes": len(own_classes), "java_bytecode_identical_to_previous_baseline": True,
          "previous_baseline_preserved": True, "native_scope": "core only; optional algorithm libraries absent"}
(root / "builds/baseline/native-core-artifact-verification.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result, indent=2))
