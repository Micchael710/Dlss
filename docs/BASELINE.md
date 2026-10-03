# Baseline de compilación - recuperación 2026-10-02

## Baseline vigente — AMD FSR FG x2 PASS

COMPLEMENTARY_OPENGL=PASS; VULKAN_INITIALIZATION=PASS; GL_VK_INTEROP=PASS;
AMD_FSR_3_1_4_CONTEXT=PASS; AMD_FSR_FG_GENERATED_FRAME=PASS; AMD_FSR_FG_X2=PASS.
Instancia aislada Minecraft1.21.1/NeoForge21.1.219, Complementary, SR1.7, Vulkan,
wisteria:fsr, X2, OptiScaler OFF. 4912 real/4912 generated presentados, orden
GENERATED(intervalo N)→REAL(N) confirmado, 4911 dispatch sin reset. Sin crash ni
errores nuevos en el run final; dos intentos anteriores conservados y corregidos.
Observación visual breve completa/HUD correcto/sin negro/corrupción/flicker anormal.
No STRESS TESTED, HDR, DH, SSRD ni multiplicadores mayores.

SR fg-backpressure SHA256 20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896,
32,331,533 bytes, 1385 clases propias major65. Core nativo original intacto.
Wisteria fsr-backpressure SHA256 ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53,
2,852,592 bytes, 31 clases propias major65. Binarios automáticos:
bridge b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b;
AMD Vulkan c84ca40421ff5594edfc6553cb89a2d07982b0cadb1a66beb989b2cdedd39197.
Notices AMD incluidos. SDK1.1.4 oficial/FSR3.1.4; provider effect ABI consultada
1.1.3 no equivale a la versión comercial de FSR. Authenticode local NotSigned.

Métricas/evidencia en logs/runtime/fsr-x2-result.json, fsr-x2-pass.log y fsr-x2-world.jpg.
Commits locales SR e98b3fdb y Wisteria 322ff7a. Sin remotes, push/PR o cambios en
.minecraft real. Detenido tras x2; revisar resultados antes de continuar.

Las secciones posteriores son el historial de recuperación, no el estado vigente.

Último gate gráfico: **Complementary ACTIVE, WORLD LOADED, FAILED_VISUAL_GATE**. Config SR versionada procesada, escena parcial y GL_INVALID_ENUM internal format. Esto no invalida compilación/core/OpenGL sin shader ni equivale a crash. Hashes JAR/core/original shader revalidados intactos. No rebuild, nuevos commits, correcciones, resize/fullscreen o Vulkan. Informe COMPLEMENTARY_OPENGL_TEST.md; baseline gráfica conjunta todavía ABIERTA.

Estado posterior vigente: **CORE_RUNTIME_PASSED_OPENGL**. SR/Wisteria INITIALIZED y Minecraft/menu/mundo acreditados. Nueva baseline `-native-core.jar`: SHA256 51c22ce1517002ade6bd891e1d104fed50348e8ffc28dc2f5cc8f557482593e0; core80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec. Baseline anterior y fallo conservados como historial. Toolchain/submódulos/exports/procedencia y límites en RUNTIME_BASELINE.md. JNI commit local03c0e447, sin push ni rebuild innecesario. Complementary/Vulkan/interop/FG/validación visual completa pendientes.

## Regla de evidencia

COMPILED requiere build exitoso y artefacto identificado. INITIALIZED, RAN IN MINECRAFT, GENERATED FRAMES, VISUALLY VALIDATED y STRESS TESTED requieren evidencia separada. Ninguno se deriva de Gradle BUILD SUCCESSFUL.

## Actualización runtime posterior — 2026-10-02

**FAILED_RUNTIME_GATE** en instancia aislada runtime-baseline/instance. Ambos artefactos mantienen COMPILED/Java21 y hashes, pero SR no inicializa: falta el recurso embedded obligatorio lib/libSuperResolution+win64+release.dll. Wisteria alcanza su entrypoint, no acredita registro/evento/providers completos. No se confirma menú ni mundo ni Vulkan/interop. Detalles/evidencia en RUNTIME_BASELINE.md y logs/runtime/baseline-errors.log. No se reempaquetó el JAR ni se usó la DLL antigua. Fases SDK/metadata suspendidas por gate explícito del usuario.

## Intentos heredados Super Resolution

1. logs/baseline/superresolution-neoforge-1.21.1.log: fallo al abrir configs/1.json. Pasar la propiedad de versión como argumento completo `-Pminecraft_version_config=1.21.1`.
2. logs/baseline/superresolution-neoforge-1.21.1-attempt2.log: Fabric Loom 1.18.2 requiere JVM 25 y recibió 21; error posterior annotationProcessor es consecuencia de configuración fallida.

## Intentos de esta iteración

SR attempt3 / Wisteria attempt1: wrapper falla antes de configurar, intentando crear bloqueo bajo C:/.gradle por entorno restringido. Sin evidencia de compilación.

SR attempt4: invocación directa de GradleWrapperMain con Java 25.0.4, caché explícita C:/Users/micha/.gradle, propiedad de versión completa y :neoforge:build. Evita el pause interactivo presente en gradlew.bat. Toolchain upstream selecciona Java 21 desde configs/1.21.1.json; no se cambia el target para satisfacer Loom.

Wisteria attempt2: Java 21.0.12, misma caché explícita, -Pmc=1.21.1 :neoforge:build. Su Loom 1.16.3 pasa configuración sin el error de JVM anterior.

NeoForge upstream configurado: 21.1.219 en ambos proyectos. No se altera a 21.1.251 sin necesidad demostrada. No se ejecuta runClient: las dependencias de desarrollo upstream incluyen DH y otros mods que deben excluirse del perfil mínimo antes de validación gráfica.

## Reproducibilidad

Los logs conservan el resultado completo. La caché Gradle requiere escritura fuera del workspace y descargas; se solicitó autorización de ejecución ampliada después del fallo restringido. Sin publicación Maven remota, sin push, sin PR. No se usaron los JAR de referencia como dependencia de compilación.

## Continuación: selección del loader

SR attempt4 terminó BUILD FAILED: Fabric no puede remapear dos mods ajenos al target NeoForge. Se añadió propiedad opcional `-Ploader=neoforge` a settings de ambos proyectos, validada contra los loaders admitidos. Sin propiedad se conserva la selección upstream. Cambio en commits locales SR 195430dc / Wisteria aeed1ef. El cambio Fabric heredado quedó preservado en SR 02b36fe3 antes de esa modificación.

SR attempt5: Java 21, `-Pminecraft_version_config=1.21.1 -Ploader=neoforge :neoforge:build`. El log confirma únicamente NeoForge. BUILD FAILED en 45s: Windows bloquea el fichero temporal descargado para `org.lwjgl:lwjgl:3.3.4:natives-windows-x86` desde Maven Central con «file contains a virus or potentially unwanted software». Fallan common y neoforge createMinecraftArtifacts. No se desactivó antivirus, no se añadieron exclusiones, no se sustituyó la biblioteca ni se alteró la gráfica. No se infiere de este mensaje si es detección correcta o falso positivo. La consulta de Get-MpThreatDetection no proporcionó detalles.

Wisteria attempt2 interrumpido antes de compilar, después de remapeo Fabric, para usar el perfil aislado. attempt3 usa `-Pmc=1.21.1 -Ploader=neoforge :neoforge:build` con Java 21. Preparación NeoForm iniciada; resultado se registra al terminar.

## Resultado final de esta iteración

Wisteria attempt3: **COMPILED**, BUILD SUCCESSFUL en 3m 6s, código de salida 0. JAR: `wisteria/neoforge/build/libs/wisteria-neoforge-1.21.1-0.1.0-alpha.1+1.21.1.jar`, 54110 bytes. SHA-256 `E41E1CF694C2E0A2724E7C887BED44D5C35818616A4B3A54C6561DAFC4AF0D29`. Todas las clases del JAR tienen major version 65 (Java 21). No contiene clases backend/fsr ni DLL/SO, a diferencia de la referencia modificada. Es build upstream con modificación de selección de loader, no un backend FG AMD.

Gradle test de Wisteria: NO-SOURCE; no se ejecutaron tests unitarios. git diff --check pasó en ambos repositorios. No INITIALIZED, RAN IN MINECRAFT, GENERATED FRAMES, VISUALLY VALIDATED ni STRESS TESTED. SR sigue NO COMPILED por bloqueo antivirus; no se repite la baseline ya compilada de Wisteria sin nuevas razones.

Los cuatro SHA-256 de referencias se comprobaron de nuevo al terminar y coinciden con el inventario inicial.

## Baseline conjunta cerrada: diagnóstico seguro y build x64

La sección anterior refleja la primera iteración. El bloqueo LWJGL quedó resuelto seleccionando explícitamente las dependencias compatibles con Windows x64, sin cambiar versiones ni código gráfico. Diagnóstico, detección y límites en LWJGL_BLOCK_DIAGNOSIS.md.

SR commit `e365f5b32df88be8fb9925aaf0491658ebf4f705` (dev). Build attempt6, salida 0, BUILD SUCCESSFUL en 4m 21s. Perfil: Java 21.0.12, Minecraft 1.21.1, loader NeoForge, NeoForge 21.1.219, OpenGL, is_dev=true y `sr_native_arch=windows-x64`. Es baseline de código upstream con cambios locales declarados en configuración del build; no es build upstream íntegramente sin modificaciones.

Invocación reproducible (desde superresolution, sin el pause de gradlew.bat):

```powershell
& 'D:\Programs File2\Eclipse Adoptium\jdk-21.0.12.101-hotspot\bin\java.exe' -classpath gradle\wrapper\gradle-wrapper.jar org.gradle.wrapper.GradleWrapperMain --gradle-user-home 'C:\Users\micha\.gradle' '-Pminecraft_version_config=1.21.1' '-Ploader=neoforge' '-Psr_native_arch=windows-x64' :neoforge:build --console=plain
```

SR JAR: `builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.e365f5b3.opengl.jar` (29802287 bytes).
SHA-256 `14BEDCFE2DE3936E3485E441453896FC1F55B55CC222043947A9AA72D8E0B882`.
ZIP CRC comprobado. Sus 1420 clases propias tienen major 65 (Java 21). Los cinco JAR embebidos también pasan CRC y usan major 52/53, compatible con Java 21. Metadata neoforge.mods.toml y librerías jarJar verificadas: imgui 1.90.0, lwjgl-vulkan 3.3.4, VMA 3.3.4 con empaquetado existente.

Wisteria baseline anterior reutilizada, sin repetir compilación: `aeed1ef78e23ef0f8de51ad65d8804af17457f61`. JAR copiado a builds/baseline con hash idéntico E41E1CF694C2E0A2724E7C887BED44D5C35818616A4B3A54C6561DAFC4AF0D29 y major 65. Ambos resultados registrados en builds/baseline/artifact-verification.json.

Auditoría offline (lwjgl-x64-artifact-audit.log) termina BUILD SUCCESSFUL y LWJGL_AUDIT_OK para common/neoforge: conserva org.lwjgl:lwjgl 3.3.4 sin classifier y natives-windows 3.3.4; no resuelve natives-windows-x86/arm64 en la configuración afectada. No se limpió caché ni usó --refresh-dependencies, porque no había corrupción/metadata obsoleta. Sin exclusiones, restauración de cuarentena, cambio de versión ni sustitución manual.

SR :neoforge:test NO-SOURCE. Este build no invocó :common:test; no atribuirle los tests de interop existentes. Evidencia adicional: auditoría funcional de selección de artifacts, integridad ZIP, bytecode y git diff --check. Sin prueba runtime.

| Nivel | SR | Wisteria |
|---|---|---|
| COMPILED | Sí, perfil Windows x64 explícito | Sí, baseline anterior |
| INITIALIZED | No verificado | No verificado |
| RAN IN MINECRAFT | No | No |
| GENERATED FRAMES | No verificado | No verificado |
| VISUALLY VALIDATED | No | No |
| STRESS TESTED | No | No |

Ningún DLL propio SR se encuentra en la raíz del JAR; VMA natives están en su JAR embebido. El proyecto conserva enable_auto_download=true y el comportamiento upstream para runtimes externos. Build COMPILED no verifica descarga/carga de esos runtimes ni disponibilidad FG en el hardware real.


## Cierre de fase A — transiciones verificadas, 2026-10-02

COMPLEMENTARY_OPENGL = PASS. PHASE_A = CLOSED.

La continuación reutilizó el JAR phase-a-fix sin reconstruir SR, Wisteria ni el core.
JAR SHA-256: a0a345486e59968dde0b60812e63e1420223ca12e07d758b2af281cfd72691b9.
Core original sin cambios: 80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec.

| Transición | Display | Render a ratio 1.7 | Resultado |
|---|---|---|---|
| Resize: 854×480 → ventana maximizada | 1920×1009 | 1129×593 | PASS |
| Ventana → fullscreen | 1920×1080 | 1129×635 | PASS |
| Fullscreen → ventana | 1920×1009 | 1129×593 | PASS |

En cada transición: escena completa, HUD/crosshair centrados, sin zonas negras ni
clipping de composición observado, sin crash y sin nuevos mensajes GL de error.
Iris conserva enableShaders=true y Complementary-SuperResolution-Wisteria.zip.
SR conserva enable_upscale=true y upscale_ratio=1.7. Los logs muestran dispatch,
inputs de tamaño render, output de tamaño display, viewport completo y FBO COMPLETE.
No representa un stress test ni una auditoría completa de contenido del shader.

Evidencia por transición: logs/runtime/phase-a-resize.{jpg,log},
phase-a-fullscreen.{jpg,log}, phase-a-return-windowed.{jpg,log}.
Resultado recuperable: logs/runtime/phase-a-transition-progress.json.

La corrección previa restauró la comprobación de dimensiones en
ShaderCompatTextureInfo.canUseSourceTextureDirectly: la región -1 se copia a un
input de render-size cuando la textura contenedora es de display-size. FSR1
normaliza usando render-size; recibir el contenedor completo causaba la escena
parcial. El control previo ratio1.0 confirmó RENDER_SIZE_DEPENDENCY_CONFIRMED.
composite7 sí existe/compila/ejecuta y su hook BEFORE sí despacha SR; se descarta
como causa demostrada la hipótesis anterior de pass ausente.

Error GL anterior: glTextureStorage2D, formato sin tamaño GL_DEPTH_COMPONENT
(0x1902), creación interna depthtex 502×282 en ShaderCompatTextureInfo.updateTexture,
vía GlTexture2D.allocateTextureStorage. El resolver ahora consulta precisión/tipo
reales: profundidad 24-bit unsigned normalized; reserva GL_DEPTH_COMPONENT24
(0x81a6). RGBA16F no era la causa. Logs/capturas previos se conservan.

Siguiente fase autorizada: inicialización Vulkan en runtime-baseline con la copia
OpenGL preservada; luego interop y entradas FG si los gates pasan. AMD dispatch y
generated frames = NO. .minecraft real intacto; sin DH/SSRD, push ni publicación.

## Vulkan inicializado — preflight de interop, 2026-10-02

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

