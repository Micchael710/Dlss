# Baseline runtime aislada — 2026-10-02

Estado posterior vigente: **CORE_RUNTIME_PASSED_OPENGL**. El fallo siguiente se conserva como historial. SR/Wisteria INITIALIZED, RAN IN MINECRAFT y WORLD LOADED = YES; Complementary, Vulkan/interop y validación visual completa siguen pendientes. Ver recuperación al final.

Resultado Complementary posterior: **PACK_ACTIVE / WORLD_LOADED / FAILED_VISUAL_GATE**. SR procesó superresolution.v1.json; escena reducida en esquina inferior izquierda/negro arriba-derecha y GL_INVALID_ENUM internal format. No crash observado; resize/fullscreen/Vulkan no ejecutados. VISUALLY_VALIDATED=NO, GENERATED FRAMES=NO. Detalles/evidencia/hipótesis sin corregir en COMPLEMENTARY_OPENGL_TEST.md. Este fallo gráfico es distinto de las interrupciones de Computer Use por Escape.

Resultado: **FAILED_RUNTIME_GATE**. Ambos JARs conservan COMPILED/Java21, pero la baseline conjunta no inicializa. No se avanzó a SDK AMD, prueba directa nueva ni metadatos FG. No se modificó .minecraft ni se ejecutó/cargó WisteriaNative.dll antigua.

## Instancia reproducible y aislamiento

Launcher dedicado: D:/ProjectDllsss/runtime-baseline. Game directory: D:/ProjectDllsss/runtime-baseline/instance. Gradle plugin ModDev2.0.147, Minecraft1.21.1, NeoForge21.1.219, JDK21.0.12.101-hotspot de Eclipse Adoptium, Windows x64. Se usa la caché C:/Users/micha/.gradle; assets oficiales Mojang en neoformruntime/assets, no en .minecraft. No cuentas/credenciales del launcher real: cliente de desarrollo.

El launcher no incluye ninguno de los checkouts SR/Wisteria como source mod: usa los dos JARs baseline exactos y sus hashes anteriores. Es configuración opcional independiente, sin cambio de comportamiento upstream ni de la API pública.

| Dependencia directa | Origen/versión | Motivo |
|---|---|---|
| SR baseline | builds/baseline, 0.9.2-alpha.1+dev.e365f5b3.opengl | Artefacto bajo prueba |
| Wisteria baseline | builds/baseline, 0.1.0-alpha.1+1.21.1 | Artefacto bajo prueba |
| Iris NeoForge | Modrinth oficial Maven,1.8.8+mc1.21.1 | Shader integration de este entorno; no Oculus |
| Sodium NeoForge | Modrinth oficial Maven,0.6.13+mc1.21.1 | Runtime elegido por configs/1.21.1.json; no Embeddium |
| Architectury | maven.architectury.dev,13.0.6 | Dependencia SR actual |
| jcpp | Maven Central,1.4.14 | Runtime dependency SR no JAR separado embedded |

LWJGL3.3.4 y STB3.3.3 según config upstream; misma selección de natives Windows x64, sin x86/ARM64. Se mantiene sodium.checks.issue2561=false como run upstream; no se elimina ningún capability check de provider. Iris/Sodium aportan Forgified Fabric API Base/BlockView/Renderer/Rendering Data Attachment y otras bibliotecas anidadas: aparecen como dependencias transitivas, no mods extra añadidos por nosotros.

Sin DH, SSRD, maps/minimap, Spark, Reese's, Simple Clouds, Sodium Options API independiente, modpack ni shaderpack. `auditRuntimeProfile` validó la lista resuelta. Avisos de compat mixins por mods opcionales ausentes no significan que estén instalados; compat de DH se refiere a una clase Iris presente, no a instalar DH.

Invocación local: Java21 directamente con el gradle-wrapper.jar de SR, directorio runtime-baseline, --gradle-user-home C:/Users/micha/.gradle, `auditRuntimeProfile` o `runClient`, --console=plain. El launcher requiere permiso de descarga/cache y abrir ventana. No necesita build de los mods. Preserve baseline hashes y no reutilice la instalación real para reparar esta prueba.

## Ejecución y evidencia

1. Resolución inicial falló al faltar el repositorio Architectury en el launcher. Se añadió la URL oficial ya configurada upstream; segunda resolución BUILD SUCCESSFUL. Esto fue preparación, no fallo de inicialización del mod.
2. Primer run, 15:53:41 hora local: SR NeoOpenGLVersionOverride detectó earlyWindowControl=true, lo actualizó a false en instance/config/fml.toml, mostró aviso de reinicio y su código prevé exit1. Se canceló el proceso de prueba en el diálogo y reinició conservando ese ajuste automático; log separado. No se modificó configuración del Minecraft real.
3. Segundo run, 15:55:19: Wisteria initializing; NeoForge e Iris/Sodium cargados, shaders disabled (ningún pack seleccionado).
4. 15:55:22: NativeLibManager de SR no encuentra el recurso obligatorio **lib/libSuperResolution+win64+release.dll** y registra Required dependency extraction failed. Se detuvo el cliente de prueba en el aviso de error; no se siguió a un mundo ni a AMD.

Evidencia: logs/runtime/baseline-client.log, baseline-client-first-run.log, baseline-game.log y baseline-errors.log (con números de línea del latest.log). gpu-host.json solo modelo/driver. runtime-artifact-audit.json confirma ausencia de DLL core y de WisteriaNative antigua. Exportación elimina usuario de rutas y posibles access tokens. Logs automáticos originales permanecen en la instancia aislada, excluida de control de versiones.

## GPU y configuración efectiva

- Render OpenGL: NVIDIA GeForce RTX3050Ti Laptop GPU/PCIe/SSE2, OpenGL4.6.0 NVIDIA596.49; Iris confirma renderer y driver.
- Host: también Radeon integrada31.0.21924.61, IDDCX y DisplayLink; no se supone que sean el adaptador Vulkan del juego.
- Java21.0.12 x64, NeoForge21.1.219, Minecraft1.21.1, SR/Wisteria versiones exactas descubiertas en Mod List.
- Config SR generada: presentation/backend=OPENGL, debug/skip_init_vulkan=true, upscale_algo=fsr1. Es default observado, no verificación Vulkan. En un futuro ensayo específico interop habrá que seleccionar init Vulkan explícitamente en esta instancia, después de arreglar core native. No se cambió ahora para ocultar el fallo.
- Vulkan device: **NOT_REACHED / no evidencia runtime**. GL↔VK interop: **NOT_VALIDATED**. Selected FG backend: **NOT_REACHED**.

## Estados y comprobaciones solicitadas

| Estado / comprobación | Resultado |
|---|---|
| SR INITIALIZED | NO: extracción required core bloquea preInit |
| Wisteria INITIALIZED completo | NO ACREDITADO: entrypoint inicia, pero no llega el gate conjunto/evento FG |
| RAN IN MINECRAFT | NO ACREDITADO: cliente arranca render/modloading; no menú principal confirmado |
| GENERATED FRAMES | NO PROBADO |
| VISUALLY VALIDATED | NO |
| STRESS TESTED | NO |
| API FG registrada runtime / Wisteria recibe evento | NO ACREDITADO: no log de Registered frame generation backends |
| wisteria:ngx / streamline inicializan o unavailable limpio | NO ACREDITADO; no eliminar checks para simularlo |
| Vulkan device e interop | NO ACREDITADO |
| Crash nativo | No hs_err/crash report observado antes de cancelación; fallo Java/recursos. No garantía de estabilidad nativa |
| DLL WisteriaNative antigua | Ausente del classpath/JARs; sin evidencia de carga |
| Mundo/fullscreen/resize/reentrada/presenter/capture ring | NO EJECUTADO por fallo anterior |

Wisteria registra listeners durante init, pero la señal final de registro depende de SR más adelante. No convertir el mensaje Wisteria initializing en prueba de callbacks ni disponibilidad.

## Causa y siguiente intervención técnica

El JAR SR bajo prueba carece del recurso obligatorio y los recursos optional nativos SR también faltan. `:neoforge:build` compila/empaqueta lo disponible; `native:buildNative` y tareas copyNativeLib(All) son explícitas y no fueron ejecutadas por el build baseline. `mustRunAfter` visto en build.gradle solo ordena tareas seleccionadas; no crea una dependencia que las ejecute. El directorio resources/lib no contiene la DLL. No hay evidencia de antivirus implicado en este fallo: **resource not found**, no download/quarantine.

ENABLE_AUTO_DOWNLOAD=1 no prueba descarga de native core. NativeLibManager.extractLibrary busca classloader.getResourceAsStream del recurso embedded; no descarga ni rescata la DLL desde .minecraft. La ausencia es comprobada estáticamente en el mismo artefacto cuyo hash se ejecutó.

Próximo trabajo: obtener/preparar core native correspondiente al source upstream actual mediante procedencia verificable, completar submódulos/toolchain/licencias, integrar su empaquetado y verificar presencia del recurso antes de nueva baseline. No copiar binarios de la referencia antigua ni cambiar required=true. No se ejecutó el script build_windows.ps1: contiene borrado de buildWindows y requiere preparación específica; no se creó DLL propia.

Fases2–4 quedan **NOT_STARTED_RUNTIME_GATE_FAILED**. La investigación estática AMD previa sigue documentada; no constituye firma Authenticode comprobada ni CONFIRMED_RUNTIME. El primer cambio de metadatos debe esperar a una prueba runtime correcta.

## Recuperación posterior — core native y continuación

`SR_MAIN_LIB` compila SRNativeMain/src y src/nvg: JNI/core API upscale, glslang, NanoVG/FreeType y serializador de vértices; no AMD FG. FreeType y glslang son CORE REQUIRED (glslang también es infraestructura de compilación). Los otros gitlinks son OPTIONAL ALGORITHM / NOT REQUIRED FOR CORE y no se inicializaron. No se ejecutó build_windows.ps1, que borra buildWindows y compila módulos opcionales en Debug y Release.

| Entrada | Procedencia/versión |
|---|---|
| FreeType | https://github.com/freetype/freetype, 5336c0d4da22a13dab3389eb153b12672fdf841c |
| glslang headers | https://github.com/KhronosGroup/glslang, a8d28bd082bff18ffbe80996e922b012f915cf07 |
| glslang static libs | URL configurada por upstream init.py: https://github.com/187J3X1-114514/glslang-prebuilt/releases/download/a8d28b/glslang-windows-release.zip |
| ZIP adquirido | 14589730 bytes, SHA256 9ec17d54fdebed4a6f851c75c02ebdc70a2cefeef44d0b25b3843f8dd860c0a0, solo Windows x64 Release |
| Toolchain instalada | Visual Studio Community2026; MSVC19.51.36244.0/toolset14.51.36231; Windows SDK10.0.26100.0 |
| Build tooling | CMake4.2.3-msvc3; Ninja1.13.2; Python para preparar glslang |
| JNI | Temurin21.0.12.1+1, include y include/win32; dependencias reales Ninja confirman que no se usó jni_win64.h vendorizado |

Core no necesita Vulkan SDK externo: usa headers vendorizados y no importa loader Vulkan. No enlaza JVM. Rutas explícitas y entorno de proceso, sin PATH global ni instalación. Release x64 /O2 /DNDEBUG /MT, conservando /ZI upstream; no CodeView/PDB path ni imports CRT debug. Fuente/entradas/toolchain identificadas; reproducción bit a bit y reproducción independiente de glslang prebuilt NO DEMOSTRADAS.

DLL: 6138880 bytes, AMD64, SHA256 **80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec**, 161 exports incluidos todos los 133 JNI esperados; solo KERNEL32.dll importado. No rutas absolutas D:/K:/, directorios personales ni PDB detectados por auditoría. Evidencia logs/native/sr-core-audit.json y dumpbin. D8003 inicial del log corresponde al sondeo cl /Bv sin fuente, no a fallo del target; script ya corregido para consultar FileVersion.

copyNativeLib upstream copió output/bin a common/src/main/resources/lib; después :neoforge:build SUCCESS19s con perfil1.21.1/NeoForge/windows-x64/Java21. Artefacto nuevo separado **builds/baseline/super_resolution-neoforge-1.21..1.21.1-0.9.2-alpha.1+dev.e365f5b3.opengl-native-core.jar**, SHA256 **51c22ce1517002ade6bd891e1d104fed50348e8ffc28dc2f5cc8f557482593e0**. Recurso lib/libSuperResolution+win64+release.dll idéntico a DLL auditada. Los 1380 classfiles del namespace propio SR son Java21 y byte a byte iguales a baseline anterior (otros namespaces/nested JARs no incluidos en ese conteo). Baseline anterior preservada.

Source: e365f5b3 más opción JNI configurable; revisión de git status/diff/diff--check confirmó únicamente esos dos cambios. Commit local posterior **03c0e447d37a8369f5e8864199bd971ec3a03f4b**, `build: use system JDK JNI headers for native core`. Sin rebuild tras commit: las fuentes ya compiladas no cambiaron. Versión Java/SRLIB_VERSION embedded conservan e365f5b3; se documenta la distinción. Sin push/PR/fork/migración.

Runtime 16:29:51: core extraído/cargado; registro Wisteria wisteria:ngx y wisteria:streamline. 16:29:53: reflex_streamline registrado. 16:30:57: jugador en mundo, servidor integrado. 16:31:59–16:32:02: cierre/guardado ordenado sin crash observado. INITIALIZED/RAN IN MINECRAFT/WORLD LOADED = YES para OpenGL. FG OFF y low latency NONE; negociación fg=null/lowLatency=null. Registro no acredita disponibilidad. GENERATED FRAMES/VISUALLY VALIDATED/STRESS TESTED = NO PROBADOS.

Continuación: cliente cerrado; relanzado el mismo runtime aislado sin reconstruir artefactos. Shader copiado a instance/shaderpacks; hash original/copia idéntico **680e76ee11b0513c84602c3a3e151c64c236c5bc952fff4492265b8d4a4cf7d7**. Original intacto. superresolution.json raíz y shaders/superresolution.json definen autotex3/depthtex/autotex2 y BEFORE composite7. Archivos no prueban activación ni resolución real de texturas. Log nuevo complementary-opengl-client.log; prueba visual pendiente de captura correcta: herramienta mostró otra ventana, no se actuó sobre esos pixels. Usuario pide prueba en pantalla secundaria. Config OpenGL funcional permanece sin modificar, skip_init_vulkan=true. Primero pasar Complementary antes de Vulkan.


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

