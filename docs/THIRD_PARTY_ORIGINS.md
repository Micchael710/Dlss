# Procedencia, licencias y pendientes — 2026-10-02

Recuperación posterior core SR_MAIN_LIB desde e365f5b3 con opción JNI luego commit local03c0e447. Únicos gitlinks inicializados: FreeType oficial5336c0d4da22a13dab3389eb153b12672fdf841c; glslang Khronos a8d28bd082bff18ffbe80996e922b012f915cf07. Bibliotecas glslang estáticas del release configurado por init.py: https://github.com/187J3X1-114514/glslang-prebuilt/releases/download/a8d28b/glslang-windows-release.zip, hash9ec17d54fdebed4a6f851c75c02ebdc70a2cefeef44d0b25b3843f8dd860c0a0. Ese hash no prueba reproducción independiente/firma. Notices de dependencias internas SPIRV-Tools/redistribución siguen pendientes antes de publicar.

MSVC19.51.36244/toolset14.51.36231, Windows SDK10.0.26100.0, CMake4.2.3-msvc3, Ninja1.13.2, Temurin21.0.12.1+1. Se usaron headers JNI del JDK instalado, verificados en dependencias del compilador, no Oracle vendorizado. No se copiaron headers JNI al JAR ni DLLs antiguas. Core80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec/JAR51c22ce1517002ade6bd891e1d104fed50348e8ffc28dc2f5cc8f557482593e0. Fuente/build/auditoría en RUNTIME_BASELINE.md y logs/native. Sin afirmación de firma del core local. AMD FG no incorporado.

Inventario del checkout y baseline, no autorización de publicación. No se retiró ningún header/LICENSE/NOTICE. La auditoría distributiva está **INCOMPLETE**: todavía hay fuentes nativas/submódulos y notices no disponibles. Mantener componentes opcionales separados de lo realmente incluido en el JAR.

Actualización runtime 2026-10-02: SR baseline no contiene lib/libSuperResolution+win64+release.dll y falla antes de inicializar. No se incorporó ninguna DLL de referencia ni se construyó native. La procedencia, build/source correspondiente y licencias del futuro core empaquetado deben registrarse antes de repararlo. AMD1.1.4 sigue solo tag recomendado: commit completo/package/assets/hash/firma/notices de archivos específicos NO OBTENIDOS por gate runtime fallido. No se ha comprobado Authenticode de ninguna DLL AMD.

## Bases y modificaciones nuestras

| Componente | Repositorio / base | Licencia vista | Modificación local |
|---|---|---|---|
| Super Resolution | https://github.com/IReallyWantToSleep/superresolution; dev `173f809f92f5e88cc3f0a1c8d1b4ec695a3b4d19` | Root LICENSE GPLv3; headers GPL-3.0-or-later | Fabric remap generado, selección loader opcional, perfil opcional sr_native_arch=windows-x64. HEAD e365f5b32df88be8fb9925aaf0491658ebf4f705 |
| SR native | Mismo upstream/base, native/LICENSE_NATIVE | LGPLv3 como texto del directorio; revisar headers de cada target y excepciones | Ningún cambio gráfico/nativo nuestro |
| Wisteria | https://github.com/IReallyWantToSleep/Wisteria; main `5997653a3bddbc15c10ec9a619791fde25151a00` | gradle.properties GPL-3.0-or-later y headers GPL3+; falta root LICENSE en checkout | Selección loader opcional. HEAD aeed1ef78e23ef0f8de51ad65d8804af17457f61 |
| AMD FG propuesto | https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK tag v1.1.4, release c6efa6b abreviado | MIT oficial, pin completo pendiente de obtener package | Ninguno; SDK FG no incorporado |
| Headers AMD SRNativeFSR4 existentes | Repositorio AMD anterior, `60f4ea81909200d8542eca14dccb2628b763a9a3`, SDK2.3.0 | MIT con copyright en headers; native/cpp/LICENSE-FFXSDK.txt | Ninguno nuestro; es upstream DX12 upscale, no nuestro FG |
| Referencias antiguas/shader | JARs y Complementary en Apparatia Alpha, hashes en REFERENCE_INVENTORY.md | Procedencia/licencia exacta de modificaciones custom y shader pendiente | Solo lectura; no redistribución ni incorporación |

GPL: preservar avisos, licencia y modificaciones identificadas; si se distribuye nuestro JAR derivado, proporcionar Corresponding Source correspondiente, incluidos scripts necesarios de build, mediante una vía válida de GPLv3. Tener un enlace al autor no reemplaza automáticamente esa obligación. LGPL exige conservar licencia/avisos y cumplir condiciones de reemplazo/relink y source de biblioteca según combinación. Revisar alcance por archivo antes de combinar binarios restrictivos; no declarar todas las DLL MIT por usar headers MIT. [Texto GNU GPL/LGPL](https://www.gnu.org/licenses/lgpl%2Bgpl-3.0-standalone.html), [FAQ GNU](https://www.gnu.org/licenses/gpl-faq.en.html).

## Submódulos declarados de SR

El índice Git contiene los siguientes gitlinks; directorios locales vacíos al inspeccionar. No se ejecutó actualización ni descarga. La ausencia de fuentes no impidió el build Java anterior, pero impide afirmar build nativo íntegro desde nuestras fuentes locales.

| Path | Origen | Commit fijado | Licencia / pendiente |
|---|---|---|---|
| native/cpp/SRNativeFSR/SDK | 187J3X1-114514/FidelityFX-SDK-UpscaleOnly | 102d79b0ef7f8b39dfab497686b8bf7530e400c3 | Derivado AMD; MIT esperada y notice ffxfsr existente; confirmar contenido exacto. No autoridad técnica FG oficial |
| native/cpp/SRNativeMain/third_party/freetype | freetype/freetype | 5336c0d4da22a13dab3389eb153b12672fdf841c | LICENSES exactas MISSING; no elegir FTL/GPL a ciegas |
| native/cpp/SRNativeMain/third_party/glslang | KhronosGroup/glslang | a8d28bd082bff18ffbe80996e922b012f915cf07 | Aviso compuesto en resources/licenses/glslang.txt; faltan fuentes/external notices exactos |
| native/cpp/SRNativeNGX/third_party/DLSS | NVIDIA/DLSS | a291cc7d2cc642a51566f3dfd5376f635cd1b284 | NVIDIA RTX SDK license existente ngx.txt; términos del commit/binario exacto pendientes |
| native/cpp/SRNativeXeSS/third_party/XeSS | intel/xess | 8fe81bdbbaf00b3c1b733fd0d830c333dc84e6f0 | Intel Simplified Software License en xess.txt; third-party-software de distribución exacta pendiente |

## Incorporados Java, shaders, fonts y headers

Fuente de avisos: superresolution/common/src/main/resources/licenses/*.txt, material/LICENSE, headers third_party. No se modificó ninguna de estas librerías. SHAs de código vendorizado no publicados por el checkout deben registrarse como MISSING en un futuro lock/provenance, no inventarse a partir de nombre o versión.

| Componente / origen | Versión o evidencia local | Licencia y obligación identificada |
|---|---|---|
| imgui-java / SpaiR/imgui-java | Tres embedded jars 1.90.0 | MIT; conservar permiso/copyright; inventariar Dear ImGui y otros notices transitivos de natives si se incorporan. [Licencia del tag](https://github.com/SpaiR/imgui-java/blob/v1.90.0/LICENSE) |
| LWJGL / LWJGL/lwjgl3 | embedded lwjgl-vulkan3.3.4, lwjgl-vma3.3.4 merged | BSD-3-Clause; reproducir notices en binario. [LICENSE3.3.4](https://github.com/LWJGL/lwjgl3/blob/3.3.4/LICENSE.md). STB3.3.3 es configuración upstream, no cambio nuestro |
| Vulkan Memory Allocator / AMD GPUOpen | Natives dentro merged VMA; pin transitive MISSING | MIT; verificar notice de versión integrada, además del binding LWJGL. [Licencia oficial](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/LICENSE.txt) |
| jcpp / org.anarres | dependency1.4.14; jcpp.txt local | Apache2.0; conservar licencia/atribuciones y NOTICE si distribución original incluye; no embedded jar separado en baseline |
| Material GUI Google | src/material/LICENSE | Apache2.0; conservar avisos y marcar modificaciones propias si ocurren |
| Yoga / Facebook + Java Applied Energistics | src/thirdparty/yoga, yoga.txt | MIT; avisos específicos del port Java deben inventariarse antes de distribución |
| NanoVG / Mikko Mononen | src/thirdparty/nanovg + nanovg.txt | zlib; no tergiversar origen, marcar cambios, preservar aviso |
| FSR1/FSR2/FFX upscale / AMD | fsr1, fsr2v221, fsr2v233, ffxfsr.txt | MIT; avisos AMD por versión, no retirar por añadir nuevo FG |
| NIS / NVIDIA | nis.txt | MIT; conservar copyright/permiso |
| SGSR / Qualcomm | sgsr.txt | BSD-3-Clause, conservar avisos/disclaimer, sin endorsement |
| Noto Sans / Adobe y autores | notosans.txt | SIL OFL1.1; conservar licencia/avisos y respetar Reserved Font Names al modificar |
| Vulkan/KHR/vk_video headers / Khronos | native/cpp/third_party | Licencias de cada header; inventario exacto pendiente, conservar SPDX/copyright |
| GLFW3.4 / glfw/glfw | glfw3.h | zlib en header; no tergiversar origen ni retirar aviso |
| GLAD / Dav1dde/glad | third_party/glad/gl.h, generado2.0.4 | SPDX exacto `(WTFPL OR CC0-1.0) AND Apache-2.0`; conservar términos de generador/especificación |
| JNI headers vendorizados / Oracle | third_party/jni/jni_win64.h mediante JNI0.h | Header declara Oracle proprietary/confidential, sujeto a términos no adjuntos; NO asumir OpenJDK/GPL Classpath Exception. Procedencia/permisos MISSING para redistribución. El bridge futuro utilizaría headers de nuestro JDK21 legítimo y registraría su licencia exacta |
| Streamline headers / NVIDIA/Streamline | third_party/Streamline/include | MIT en sl.h; los plugins DLSS-G/NGX y DLLs tienen términos separados |
| Minecraft/NeoForge/Iris/Sodium/etc. | Compile/runtime según configs y Gradle | Dependencias de entorno, no código nuestro; no redistribuir Minecraft. Inventario resuelto/SBOM completo y licencias transitivas pendiente |
| Gradle plugins/Manifold/JUnit/mixin | Dependencias build/compile/test | Herramientas, no asumir incorporadas al JAR; lock/SBOM separado pendiente |

Baseline SR contiene exactamente cinco JARs anidados (tres imgui, LWJGL Vulkan, merged VMA) y trece notices en licenses/. El inventario JSON local enumera paths. Baseline Wisteria no contiene nested jars, notices ni DLLs; compilar no prueba capacidad runtime nativa. El permiso de código fuente Wisteria sigue vigente aunque falte el texto completo empaquetado: corregir esa ausencia documental antes de publicar.

## Binarios opcionales y condiciones distributivas

ngx.txt concede usos/redistribución bajo condiciones NVIDIA específicas, pide atribución para derivados de source y regula SDK/tools; no sustituir por MIT. xess.txt permite binario sin modificación con avisos y prohíbe reverse engineering; **no se descompiló XeSS**. Streamline sync usa K:/sl/bin/x64 por defecto y siete DLLs; su comentario «redistributable but not committable» no es una licencia. Recopilar términos y notices exactos de cada DLL, incluida nvngx_dlssg/NvLowLatencyVk, y revisar compatibilidad con el conjunto GPL antes de distribuir. Ninguna DLL se copió ahora.

AMD propuesta: conservar LICENSE MIT/header y auditar package thirdparty/notices, nombre/hash/firma de DLL Vulkan. La recomendación AMD de DLL firmada es un requisito técnico de ruta soportada distinto de la licencia de fuente MIT. No usar trademark/logo para sugerir certificación.

## Pendientes que impiden declarar cierre distributivo

Fuentes/licencias completas de cinco gitlinks; texto GPL Wisteria y avisos binario; términos/procedencia del JNI Oracle vendorizado; versiones/notices transitivos embedded y fuente nativa prebuilt SR; SDK FG exacto y package notices; compatibilidad distributiva NVIDIA/Intel opcional con GPL; provenance/licencia shader si más adelante se desea redistribuir. No se afirma que cada biblioteca esté ya aprobada para nuestro futuro GitHub. Este inventario preserva lo identificado y hace explícito lo que falta.


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

