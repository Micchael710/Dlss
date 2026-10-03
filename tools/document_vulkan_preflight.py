"""Record runtime Vulkan initialization separately from untested interop gates."""
from pathlib import Path
import hashlib
import json
import zipfile

root = Path(__file__).resolve().parents[1]
artifact = root / 'builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.03c0e447.opengl-phase-a-fix.jar'
expected = 'a0a345486e59968dde0b60812e63e1420223ca12e07d758b2af281cfd72691b9'
assert hashlib.sha256(artifact.read_bytes()).hexdigest() == expected
with zipfile.ZipFile(artifact) as archive:
    dlls = [name for name in archive.namelist() if name.lower().endswith('.dll')]
assert not any('SuperResolutionStreamline' in name for name in dlls)
log_path = root / 'logs/runtime/phase-b-vulkan-init-runtime.log'
log = log_path.read_text(encoding='utf-8')
assert 'Vulkan initialization completed' in log
assert 'VK_KHR_external_memory_win32' in log
assert 'VK_KHR_external_semaphore_win32' in log
result = {
    'complementary_opengl': 'PASS', 'phase_a': 'CLOSED',
    'vulkan_initialization': 'PASS',
    'presentation_backend': 'OPENGL', 'skip_init_vulkan': False,
    'device_selection': 'upstream UUID and driver UUID match: NVIDIA RTX 3050 Ti Laptop GPU',
    'queue_family': 0, 'main_index': 0, 'fg_index': 1, 'present_index': 2,
    'external_memory_win32': True, 'external_semaphore_win32': True,
    'timeline_semaphore': True, 'synchronization2': True,
    'gl_vk_interop': 'BLOCKED_PREFLIGHT_MISSING_REQUIRED_NATIVE',
    'interop_runtime_attempted': False, 'fg_inputs_validation': 'PENDING_PHASE_C',
    'missing_required_native': 'libSuperResolutionStreamline+win64+release.dll',
    'dlls_in_unchanged_jar': dlls, 'jar_sha256': expected,
    'rebuilt': False, 'amd_dispatch': False, 'generated_frames': False,
    'limits': ['Vulkan API/driver numeric versions and logical device handle are not logged',
               'No external-image format/usage support test, export/import or semaphore round trip yet',
               'SDK not configured: VULKAN_SDK unset and usual C:/VulkanSDK, D:/VulkanSDK absent'],
}
(root / 'logs/runtime/phase-b-vulkan-progress.json').write_text(
    json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
heading = '## Vulkan inicializado — preflight de interop, 2026-10-02'
detail = '''
Estado vigente: COMPLEMENTARY_OPENGL=PASS; PHASE_A=CLOSED;
VULKAN_INITIALIZATION=PASS. GL_VK_INTEROP=BLOCKED_PREFLIGHT_MISSING_REQUIRED_NATIVE.
La fase C no se ejecutó en runtime y la validación de inputs FG de fase D queda
pendiente del gate C. No convertir capacidades Vulkan en un PASS de interop.

Se reutilizó el mismo JAR phase-a-fix (SHA-256
a0a345486e59968dde0b60812e63e1420223ca12e07d758b2af281cfd72691b9), sin
reconstruir SR/Wisteria/core ni repetir las transiciones de fase A. Sólo se cambió
skip_init_vulkan=true→false en la instancia aislada. Presentación OPENGL,
FSR1 a ratio1.7, FG OFF y low latency NONE permanecen efectivos. Mundo cargado,
Complementary activo, escena completa y HUD alineado en la captura verificada.

Runtime a las 17:41:15: «Vulkan initialization completed». El selector upstream
compara UUID de dispositivo y driver con OpenGL: coinciden en NVIDIA GeForce RTX
3050 Ti Laptop GPU (01895B66D1CA454D88788DD21FDEF638;
9C4136A706265B659812D559FF142597), y no en Radeon integrada. No selección por
vendor hardcoded. Colas main/FG/Present: familia0, índices0/1/2; handles de esta
sesión 0x1c345db23c0 / 0x1c345db2850 / 0x1c345db3170. El nombre Present de la cola
no prueba soporte de superficie: se sigue presentando con OpenGL. Se habilitan
external_memory_win32/external_semaphore_win32, timelineSemaphore y
synchronization2. Upstream solicita API1.2; versión soportada, IDs vendor/device,
versión numérica del driver, handle del device lógico y capacidades de formatos
para usos de imagen concretos no están medidos por estos logs.

Preflight C: NativeLibManager.java:74 y :108 declara la DLL
libSuperResolutionStreamline+win64+release.dll obligatoria cuando se pide
presentación Vulkan en Windows. El tercer argumento de NativeLib es required
(:419 y :430); extract() acumula y lanza requiredFailures (:197). Esto se cumple aunque
mayUseStreamline() sea false con low latency NONE y FG OFF. El JAR actual sólo
contiene el core native, no ese wrapper. No se activó una configuración cuyo
preflight demuestra una dependencia fatal ausente.

FrameCaptureManager se inicializa desde VulkanPresentationWindow.initialize();
la captura GL normal de depth/MV está condicionada a presentación Vulkan.
FrameTextureResource.ensureOwned requiere owner thread/contexto render oculto
de PresentationWindowState. Inicializar Vulkan con presentación OpenGL no prueba
la captura, el ring ni su ciclo REUSABLE→SUBMITTED→REUSABLE. El algoritmo FSR1
actual tampoco es GlVulkanInteropAlgorithm. No se fabricaron handles ni estados,
no se forzó un algoritmo nativo AMD y no se modificó el requisito required.

Siguiente trabajo concreto: resolver la dependencia del wrapper upstream en un
artefacto separado, conservando este JAR/core. El target oficial
native/cpp/SRNativeStreamline/CMakeLists.txt usa fuente/headers ya presentes y
find_package(Vulkan REQUIRED); VULKAN_SDK no está configurado y las ubicaciones
habituales C:/VulkanSDK y D:/VulkanSDK no existen. No se ha configurado/compilado
ese target ni adquirido un SDK. Alternativa de código requiere justificar primero
si exigir extracción del wrapper cuando no se carga es correcto; no aplicar un
cambio silencioso para falsear el gate. Una vez resuelto, activar presentación
Vulkan con FG OFF y registrar imágenes, formatos/usos, semáforos y lifecycle antes
de validar inputs FG. No AMD dispatch/generated frames/DH/SSRD.

Evidencia: logs/runtime/phase-b-vulkan-init-runtime.log,
phase-b-vulkan-init-client.log, phase-b-effective-config.toml,
phase-b-vulkan-init-world.jpg y phase-b-vulkan-progress.json. La copia OpenGL
validada está en opengl-config-phase-a-pass.toml. Las limitaciones de arriba son
explícitas: VULKAN_INITIALIZATION PASS no cierra toda la baseline gráfica conjunta.

Commits locales del cambio ya validado: ead2740eb60895ff986d2dd6422f8cd5b4cf25c0
(diagnóstico) y 436b50a2a5c90ae8b4e50231e348891d2eeae01b (fix de región/depth).
Sin nuevos cambios de fuente en esta continuación, sin push ni PR. .minecraft
real permanece intacto. Los remotes de origen conservan push deshabilitado;
no se afirma que ya se hayan desconectado completamente.
'''
names = ['CURRENT_STATE.md', 'BASELINE.md', 'DEVELOPMENT_LOG.md',
         'COMPLEMENTARY_OPENGL_TEST.md', 'RUNTIME_BASELINE.md',
         'FRAME_GENERATION_ARCHITECTURE.md', 'FSR_PROVIDER_CONTRACT.md',
         'AMD_FSR_FG_PLAN.md', 'THIRD_PARTY_ORIGINS.md']
for name in names:
    path = root / 'docs' / name
    previous = path.read_text(encoding='utf-8')
    if heading in previous:
        continue
    block = heading + '\n' + detail + '\n'
    path.write_text(block + '\n' + previous if name == 'CURRENT_STATE.md'
                    else previous.rstrip() + '\n\n' + block, encoding='utf-8')
print(json.dumps(result, indent=2, ensure_ascii=False))
