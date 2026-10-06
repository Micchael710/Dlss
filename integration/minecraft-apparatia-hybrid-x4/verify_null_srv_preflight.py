import hashlib
import json
import os
import re
import zipfile

def run():
    results = {}

    # 1. Bridge build pass
    build_dll = r"D:\ProjectDllsss\wisteria\native\build\dlssg-windows-x64\wisteria_dlssg_bridge.dll"
    results["build_dll_exists"] = os.path.exists(build_dll)
    build_hash = hashlib.sha256(open(build_dll, "rb").read()).hexdigest().lower()
    results["new_bridge_sha256"] = build_hash

    # 2. JAR bridge hash match
    jar_path = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\mods\wisteria-sr-x2-handoff-4d720e9b98f0.jar"
    with zipfile.ZipFile(jar_path, "r") as z:
        jar_bridge_bytes = z.read("natives/windows-x64/wisteria_dlssg_bridge.dll")
        jar_hash = hashlib.sha256(jar_bridge_bytes).hexdigest().lower()
    results["jar_bridge_sha256"] = jar_hash
    results["jar_bridge_hash_match"] = (build_hash == jar_hash)

    # 3. dlss-x4 native copy
    x4_native_path = os.path.join(r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x4\native", f"wisteria_dlssg_bridge-{build_hash}.dll")
    results["x4_native_exists"] = os.path.exists(x4_native_path)
    if results["x4_native_exists"]:
        x4_native_hash = hashlib.sha256(open(x4_native_path, "rb").read()).hexdigest().lower()
        results["x4_native_hash_match"] = (build_hash == x4_native_hash)

    # 4. X2 backup intact
    x2_version = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x2\component\version.dll"
    x2_nvngx = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x2\stock-runtime\nvngx_dlssg.dll"
    x2_ver_hash = hashlib.sha256(open(x2_version, "rb").read()).hexdigest().lower() if os.path.exists(x2_version) else None
    x2_nvngx_hash = hashlib.sha256(open(x2_nvngx, "rb").read()).hexdigest().lower() if os.path.exists(x2_nvngx) else None
    results["x2_backup_intact"] = (
        x2_ver_hash == "c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838" and
        x2_nvngx_hash == "ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82"
    )

    # 5. SM86 INI MaxGeneratedFrames=1
    ini_path = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x4\component\dlssg_sm86.ini"
    with open(ini_path, "r", encoding="utf-8") as f:
        ini_content = f.read()
    m_mgf = re.search(r"MaxGeneratedFrames\s*=\s*(\d+)", ini_content)
    results["sm86_max_generated_frames"] = int(m_mgf.group(1)) if m_mgf else None

    # 6. Hybrid requestedCount=3 in JVM
    tl_path = r"C:\Users\micha\AppData\Roaming\.tlauncher\minecraft_tlauncher_java_config.json"
    with open(tl_path, "r", encoding="utf-8") as f:
        tl = json.load(f)
    req_count_arg = [a for a in tl["jvm"]["2"]["args"] if "requestedCount" in a]
    results["jvm_requested_count"] = req_count_arg[0] if req_count_arg else None

    # 7. config.toml mode=X4, provider=wisteria:dlssg_fg
    cfg_path = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\config\super_resolution\config.toml"
    with open(cfg_path, "r", encoding="utf-8") as f:
        cfg = f.read()
    m_mode = re.search(r'\[frame_generation\][\s\S]*?mode\s*=\s*"([^"]+)"', cfg)
    m_prov = re.search(r'\[frame_generation\][\s\S]*?provider\s*=\s*"([^"]+)"', cfg)
    results["config_mode"] = m_mode.group(1) if m_mode else None
    results["config_provider"] = m_prov.group(1) if m_prov else None

    # 8. Stale failure preserved
    archived_path = r"D:\ProjectDllsss\logs\research\minecraft-hybrid-x4-null-srv-fix\previous-integration-failure.txt"
    results["stale_failure_preserved"] = os.path.exists(archived_path)
    current_failure_path = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\local\dlss-x4\integration-failure.txt"
    results["current_failure_removed"] = not os.path.exists(current_failure_path)

    print(json.dumps(results, indent=2))
    return results

if __name__ == "__main__":
    run()
