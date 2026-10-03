"""Read-only PE/exports/dependencies and JNI include provenance audit."""
import hashlib
import json
import re
import struct
from pathlib import Path

workspace = Path(__file__).resolve().parents[1]
cpp = workspace / "superresolution/native/cpp"
dll = cpp / "output/bin/libSuperResolution+win64+release.dll"
blob = dll.read_bytes()
pe = struct.unpack_from("<I", blob, 0x3C)[0]
assert blob[:2] == b"MZ" and blob[pe:pe+4] == b"PE\0\0"
machine = struct.unpack_from("<H", blob, pe+4)[0]
assert machine == 0x8664, hex(machine)
exports_text = (workspace / "logs/native/sr-core-exports.txt").read_text(encoding="utf-8-sig")
exports = re.findall(r"^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)", exports_text, re.M)
expected_jni = set()
for source in (cpp / "SRNativeMain/src").rglob("*.cpp"):
    expected_jni.update(re.findall(r"JNIEXPORT\s+\w+\s+JNICALL\s+(Java_\w+)", source.read_text(encoding="utf-8")))
missing = sorted(expected_jni - set(exports))
assert not missing, missing
dependencies_text = (workspace / "logs/native/sr-core-dependencies.txt").read_text(encoding="utf-8-sig")
dependencies = re.findall(r"^\s+(\S+\.dll)\s*$", dependencies_text, re.M | re.I)
assert dependencies == ["KERNEL32.dll"], dependencies
strings = re.findall(rb"[ -~]{6,}", blob)
path_strings = [s.decode("ascii") for s in strings if re.search(rb"^[A-Za-z]:[\\/]|[DK]:[\\/]|buildWindows|\.pdb", s)]
assert not path_strings, path_strings
ninja_deps = cpp / "buildWindowsCoreRelease/.ninja_deps"
dependency_bytes = ninja_deps.read_bytes()
assert b"jdk-21.0.12.101-hotspot" in dependency_bytes
for forbidden in (b"jni_win64.h", b"jni_linux64.h", b"jni_macarm64.h"):
    assert forbidden not in dependency_bytes, forbidden
result = {"path": str(dll.relative_to(workspace)), "architecture": "AMD64", "sha256": hashlib.sha256(blob).hexdigest(),
          "bytes": len(blob), "export_count": len(exports), "jni_export_count": len(expected_jni),
          "missing_jni_exports": missing, "exports": exports, "imported_dlls": dependencies,
          "absolute_path_strings": path_strings, "jni_headers": "Temurin JDK21 include + include/win32 (compiler dependencies verified)",
          "configuration": "Release, /O2 /DNDEBUG /MT, upstream /ZI; no CodeView PDB path or debug CRT imports",
          "reproducibility": "source/inputs/toolchain documented; bit-for-bit rebuilding not tested"}
(workspace / "logs/native/sr-core-audit.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps({k:v for k,v in result.items() if k != "exports"}, indent=2))
