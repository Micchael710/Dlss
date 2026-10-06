import hashlib
import json
import os
import re
import zipfile

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().lower()

def verify():
    results = {}

    # 1. Check config.toml
    config_path = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\config\super_resolution\config.toml"
    with open(config_path, "r", encoding="utf-8") as f:
        text = f.read()
    m = re.search(r'\[frame_generation\][\s\S]*?mode\s*=\s*"([^"]+)"', text)
    results["config_mode"] = m.group(1) if m else "NOT_FOUND"

    # 2. Check TLauncher config
    tl_path = r"C:\Users\micha\AppData\Roaming\.tlauncher\minecraft_tlauncher_java_config.json"
    with open(tl_path, "r", encoding="utf-8") as f:
        tl_data = json.load(f)
    jvm2_args = tl_data["jvm"]["2"]["args"]
    results["jvm_requested_count"] = [a for a in jvm2_args if "requestedCount" in a]
    results["jvm_run_dir"] = [a for a in jvm2_args if "runDir" in a]

    # 3. Check dlss-x2 intact
    x2_version = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x2\component\version.dll"
    x2_nvngx = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x2\stock-runtime\nvngx_dlssg.dll"
    results["x2_version_hash"] = sha256_file(x2_version)
    results["x2_nvngx_hash"] = sha256_file(x2_nvngx)
    results["x2_intact"] = (
        results["x2_version_hash"] == "c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838" and
        results["x2_nvngx_hash"] == "ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82"
    )

    # 4. Check dlss-x4 files
    x4_version = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x4\component\version.dll"
    x4_nvngx = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x4\stock-runtime\nvngx_dlssg.dll"
    x4_bridge = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x4\native\wisteria_dlssg_bridge-ba0a712cf3bc62fb131a58b5dbec8bf4429269525778fe197844daad37728ab1.dll"
    results["x4_version_exists"] = os.path.exists(x4_version)
    results["x4_nvngx_exists"] = os.path.exists(x4_nvngx)
    results["x4_bridge_hash"] = sha256_file(x4_bridge)

    # 5. Check JAR entry
    jar_path = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\mods\wisteria-sr-x2-handoff-4d720e9b98f0.jar"
    with zipfile.ZipFile(jar_path, "r") as z:
        bridge_bytes = z.read("natives/windows-x64/wisteria_dlssg_bridge.dll")
        results["jar_bridge_hash"] = hashlib.sha256(bridge_bytes).hexdigest().lower()

    # 6. Check rollback readiness
    backup_dir = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x4-backup-manifest"
    results["backup_manifest_exists"] = os.path.exists(os.path.join(backup_dir, "backup-manifest.json"))
    results["rollback_txt_exists"] = os.path.exists(os.path.join(backup_dir, "ROLLBACK.txt"))

    print(json.dumps(results, indent=2))
    return results

if __name__ == "__main__":
    verify()
