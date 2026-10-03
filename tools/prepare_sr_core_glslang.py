"""Install only the upstream-pinned Windows x64 Release glslang package."""
import hashlib
import importlib.util
import json
from pathlib import Path

workspace = Path(__file__).resolve().parents[1]
cpp = workspace / "superresolution/native/cpp"
spec = importlib.util.spec_from_file_location("sr_native_init", cpp / "init.py")
upstream = importlib.util.module_from_spec(spec)
spec.loader.exec_module(upstream)
log_dir = workspace / "logs/native"
log_dir.mkdir(parents=True, exist_ok=True)
package = "glslang-windows-release.zip"
url = f"{upstream.GLSLANG_RELEASE_BASE_URL}/{upstream.GLSLANG_COMMIT_HASH}/{package}"
archive = log_dir / package
destination = upstream.GLSLANG_PREBUILT_ROOT / f"{upstream.GLSLANG_COMMIT_HASH}-windows-x86_64-release"
if destination.exists():
    raise SystemExit("Destination already exists; inspect it rather than replacing it")
print(f"Downloading the package configured in upstream init.py: {url}", flush=True)
upstream.download_file(url, archive)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
upstream.extract_archive(archive, destination)
missing = upstream.missing_files(destination, "windows-x86_64", "release")
if missing:
    raise SystemExit(f"Incomplete package: {missing}")
upstream.write_commit_hash_file()
manifest = {"url": url, "sha256": digest, "bytes": archive.stat().st_size,
            "configured_commit_prefix": upstream.GLSLANG_COMMIT_HASH,
            "pinned_source_commit": "a8d28bd082bff18ffbe80996e922b012f915cf07",
            "scope": "windows-x86_64-release", "independent_binary_reproducibility": "NOT_VERIFIED"}
(log_dir / "glslang-package.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print(json.dumps(manifest, indent=2))
