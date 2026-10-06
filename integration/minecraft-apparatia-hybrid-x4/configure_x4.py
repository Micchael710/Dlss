import json
import re

def update_config_toml():
    path = r"C:\Users\micha\AppData\Roaming\.minecraft\versions\Apparatia Alpha\config\super_resolution\config.toml"
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()

    # Find [frame_generation]
    pattern = r'(\[frame_generation\][\s\S]*?mode\s*=\s*")[^"]+(")'
    new_text, count = re.subn(pattern, r'\g<1>X4\g<2>', text)
    if count == 0:
        raise RuntimeError("Failed to replace mode in [frame_generation]")

    with open(path, "w", encoding="utf-8") as f:
        f.write(new_text)
    print("Updated config.toml: mode = 'X4'")

def update_tlauncher_json():
    path = r"C:\Users\micha\AppData\Roaming\.tlauncher\minecraft_tlauncher_java_config.json"
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)

    # Entry "2" is the target JVM
    jvm2 = data["jvm"]["2"]
    jvm2["args"] = [
        "-Dwisteria.dlssg.enabled=true",
        "-Dwisteria.dlssg.mode=presentation",
        "-Dwisteria.dlssg.requestedCount=3",
        "-Dwisteria.dlssg.runDir=C:/Users/micha/AppData/Roaming/.minecraft/versions/APPARA~1/local/dlss-x4",
        "-Dwisteria.dlssg.externalDll=C:/Users/micha/AppData/Roaming/.minecraft/versions/APPARA~1/local/dlss-x4/component/version.dll",
        "-Dwisteria.dlssg.runtimeDir=C:/Users/micha/AppData/Roaming/.minecraft/versions/APPARA~1/local/dlss-x4/stock-runtime",
        "-Dsr.dlss.runDir=C:/Users/micha/AppData/Roaming/.minecraft/versions/APPARA~1/local/dlss-x4"
    ]

    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    print("Updated minecraft_tlauncher_java_config.json: requestedCount=3, runDir=local/dlss-x4")

if __name__ == "__main__":
    update_config_toml()
    update_tlauncher_json()
    print("Configuration update COMPLETE.")
