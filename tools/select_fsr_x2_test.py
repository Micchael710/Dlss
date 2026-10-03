"""Retain/audit changed Wisteria JAR and select only the isolated x2 profile."""
import hashlib, json, re, shutil, struct, sys, zipfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = root / 'wisteria/neoforge/build/libs/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1.jar'
label = sys.argv[1] if len(sys.argv) > 1 else 'fsr-dispatch'
assert re.fullmatch('[a-z0-9-]+', label)
target = root / f'builds/baseline/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1-{label}.jar'
with zipfile.ZipFile(source) as jar:
    assert jar.testzip() is None
    own = [n for n in jar.namelist() if n.startswith('org/ireallywanttosleep/wisteria/') and n.endswith('.class')]
    assert all(struct.unpack_from('>H', jar.read(n), 6)[0] == 65 for n in own)
    native = {n: hashlib.sha256(jar.read('natives/windows-x64/' + n)).hexdigest()
              for n in ('amd_fidelityfx_vk.dll', 'wisteria_fsr_bridge.dll')}
    assert native['amd_fidelityfx_vk.dll'] == 'c84ca40421ff5594edfc6553cb89a2d07982b0cadb1a66beb989b2cdedd39197'
    assert 'licenses/amd-fidelityfx-1.1.4/LICENSE.txt' in jar.namelist()
digest = hashlib.sha256(source.read_bytes()).hexdigest()
if target.exists(): assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
else: shutil.copy2(source, target)
launcher = root / 'runtime-baseline/build.gradle'
text = launcher.read_text(encoding='utf-8')
text, count = re.subn(r"runtimeOnly files\('\.\./builds/baseline/wisteria[^']+\.jar'\)",
                     f"runtimeOnly files('../builds/baseline/{target.name}')", text)
assert count == 1
text = text.replace("            jvmArgument '-Dwisteria.fsr.contextProbe=true'\n", '')
launcher.write_text(text, encoding='utf-8')
config = root / 'runtime-baseline/instance/config/super_resolution/config.toml'
text = config.read_text(encoding='utf-8')
text = text.replace('enable_detailed_profiling = false', 'enable_detailed_profiling = true')
text = text.replace('mode = "OFF"\n\t#DLSS Frame Generation algorithm group', 'mode = "X2"\n\t#DLSS Frame Generation algorithm group')
text = text.replace('backend = "superresolution:auto"', 'backend = "wisteria:fsr"')
assert 'provider = "wisteria:fsr_fg"' in text and 'enabled = false' in text and 'dll_path = ""' in text
assert 'mode = "X2"' in text and 'backend = "VULKAN"' in text
config.write_text(text, encoding='utf-8')
audit = dict(file=target.name, sha256=digest, bytes=target.stat().st_size, java21_classes=len(own), natives=native,
             config='isolated instance only; SR1.7; Complementary; Vulkan; wisteria:fsr X2; OptiScaler OFF')
(root/f'logs/runtime/wisteria-{label}-artifact.json').write_text(json.dumps(audit, indent=2), encoding='utf-8')
print(json.dumps(audit, indent=2))
