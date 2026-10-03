# Plan AMD FSR Frame Generation — 2026-10-02

Estado: DESIGN_ONLY. Ninguna DLL nueva, shader AMD, JNI o dispatch implementado. Elección de investigación: **FidelityFX SDK v1.1.4, FSR 3.1.4, Vulkan, Windows x64**. Es una elección por backend e interoperabilidad, no la última versión del SDK. [Release oficial 1.1.4](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4).

Actualización runtime: **FAILED_RUNTIME_GATE**, core native SR ausente en baseline (RUNTIME_BASELINE.md). Según el gate del usuario, no se obtuvo el package AMD ni se inspeccionó firma/hash de ninguna DLL AMD en esta iteración. «DLL oficial firmada» en este plan describe la ruta recomendada por documentación AMD; NO afirma Authenticode verificado de un archivo local. Commit completo/assets/package y notices exactos siguen pendientes.

### Niveles de evidencia vigentes, sin nueva prueba directa

| Afirmación | Nivel / alcance |
|---|---|
| NO_SWAPCHAIN_CONTEXT_NOTIFY define ruta sin modificar swapchain | CONFIRMED_HEADER de investigación previa |
| Configure evita fpSwapChainConfigureFrameGeneration con esa bandera | CONFIRMED_SOURCE previo; no prueba DLL |
| Dispatch recibe commandList/outputs de aplicación | CONFIRMED_HEADER y CONFIRMED_SOURCE previos |
| No vkQueuePresentKHR oculto en todo el call graph directo | UNKNOWN: se confirmó evitar proxy configure, no una prueba runtime ni traza completa de todas las llamadas |
| Optical Flow/Frame Interpolation son internos al provider AMD | CONFIRMED_SOURCE estático previo; no son inputs de SR |
| frameID+1, reset y CameraInfo | CONFIRMED_HEADER; comportamiento exacto history/buffering bajo pipeline SR todavía UNKNOWN |
| SR PresentWorker puede administrar outputs de la DLL elegida | INFERENCE respaldada por fuente; CONFIRMED_RUNTIME ausente |
| Firma/hash/binario AMD específico correcto | UNKNOWN / package no obtenido por gate |

Fases SDK exacto y viabilidad directa nuevas pospuestas hasta resolver baseline runtime; no repetir ni elevar la investigación anterior.

El SDK 2.3.0 actual incorpora tecnologías ML y effects DLL de DX12 y remite los efectos de la línea anterior a 1.1.4. El checkout SR ya contiene headers 2.3.0 para SRNativeFSR4 DX12; no son una implementación FG Vulkan. [Releases AMD](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases).

La fuente 1.1.4 utiliza MIT, conservar copyright y permiso AMD; las herramientas/dependencias incluidas requieren su propia revisión. [LICENSE oficial del tag](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v1.1.4/LICENSE.txt). No se descargó ni copió el SDK completo.

## Componentes separados

| Componente | Función / decisión |
|---|---|
| Super Resolution | Upscaling; el mod SR conserva el upscaler elegido. FSR FG 3.1 puede trabajar con otro upscaler |
| FG Prepare | Preparar depth/motion y datos de cámara para interpolación |
| Optical Flow 1.1.2 | Estimar movimiento de imagen y cambios de escena; es compute AMD, no NV optical flow hardware |
| Frame Interpolation 1.1.3 | Combinar movimiento del juego/optical flow y construir el frame intermedio |
| Frame Pacing | Distribuir tiempos de presentación; candidato: el PresentWorker actual de SR |
| Swapchain Integration | Ruta proxy oficial AMD; alternativa directa examinada más abajo |
| UI Composition | Impedir interpolación incorrecta del HUD; no se resuelve simplemente presentando dos colores |

FG independiente de upscale y composición HUDless están descritos en el [manual FSR 3.1.4](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-interpolation/). El plan no reemplaza el upscaler ni incorpora CAS, denoiser, GI, ray regeneration, Anti-Lag, samples Cauldron o DX12 al mod.

## Ruta principal y condición de viabilidad

Candidato: APPLICATION_MANAGED_ASYNC, usando **FidelityFX API de la DLL Vulkan prebuilt firmada de AMD**. El header oficial ofrece `FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY`, y dispatch con commandList/presentColor/outputs: evidencia concreta de una ruta que no modifica el swapchain. [Header FFX FG del tag](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v1.1.4/ffx-api/include/ffx_api/ffx_framegeneration.h).

El código oficial Configure copia flags/HUDLessColor y omite fpSwapChainConfigureFrameGeneration al activar esa bandera; el dispatch registra Optical Flow e interpolación en el commandList recibido y escribe outputs[0]. Esto respalda el modelo de un generado y presentación SR. [Implementación oficial del provider 1.1.4](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v1.1.4/ffx-api/src/ffx_provider_framegeneration.cpp).

Inferencia de diseño: configurar el contexto con esa bandera y grabar prepare + interpolation en el command buffer de SR podría preservar PresentWorker como propietario. **Pendiente** revisar bookkeeping frameID/historial completo y buffering, HUDless, layouts finales, necesidades de cola y ejecución con la DLL exacta. El camino fuente sin notificación de swapchain no demuestra por sí solo todo el comportamiento runtime del binario firmado. No pasar un swapchain SR real a un proxy sin diseñar la integración.

La ruta oficialmente documentada integra un proxy swapchain Vulkan con cuatro colas distintas, tres dedicadas a FSR. AMD pide usar la DLL firmada para builds distribuidos. Si la ruta directa no cumple contratos, rediseñar el modelo de ejecución antes de implementar; no introducir presents ocultos en un provider async. [Guía oficial de integración, pp. 5, 43 y 99](https://gpuopen.com/presentations/2024/FidelityFX_Super_Resolution_3-1_Release-Overview_and_Integration.pdf).

Los headers de bajo nivel ffxFrameInterpolation/ffxOpticalflow permiten estudiar prepare/history/recursos. Compilar efectos propios desde fuente no equivale automáticamente a la integración FidelityFX API soportada por AMD. Plan principal: compilar nuestro puente, conservar la DLL AMD sin modificar. [Introducción oficial a FFX API](https://gpuopen.com/manuals/fidelityfx_sdk/getting-started/ffx-api/).

## Build y empaquetado propuestos, aún no ejecutados

1. Obtener package 1.1.4 desde el release oficial y registrar tag, SHA de fuente completo, hash de DLL, firma AMD y licencias exactas. No emplear el submódulo UpscaleOnly de SR como SDK FG.
2. Target CMake del futuro bridge Wisteria, MSVC/Visual Studio 2022, configuración Release x64, headers JNI Java21 y Vulkan/FFX API 1.1.4. Obtener funciones FFX mediante carga dinámica del archivo Vulkan firmado, no enlazar DX12 y Vulkan con símbolos duplicados. El target no recompila shaders AMD.
3. Gradle: tareas explícitas configure/build/package native por plataforma; outputs bajo build/, dependencias declaradas, sin K:/ ni rutas personales. processResources depende del resultado y falla claramente si faltan bridge/DLL/manifiesto. No repetir el NO-SOURCE silencioso de Streamline como validación runtime.
4. JAR futuro: natives/windows-x64/<bridge propio>, DLL Vulkan AMD original, manifest con SDK/ABI/hashes, y licencias/notices. Confirmar el nombre exacto de la DLL desde el package antes de fijarlo en código.
5. Extraer en directorio de cache del mod segregado por versión/hashes, escritura temporal y rename atómico; verificar hashes, cargar por ruta absoluta, restringir búsqueda de dependencias a ese directorio/sistema. No buscar ni cargar WisteriaNative.dll antigua por nombre. No escribir .minecraft en esta fase.
6. JNI realiza capacidades/versiones, create/configure/prepare/dispatch/destroy. Ninguna JNI llamada adquiere swapchain o submite/presenta a espaldas de SR en el modelo async. Java maneja leases; C++ guarda únicamente sesiones/recursos propios.

El CMake Vulkan oficial sirve como referencia de backend/permutaciones si se necesitara una investigación de fuente, no como orden de copiarlo ahora. [CMake AMD Vulkan 1.1.4](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v1.1.4/sdk/src/backends/vk/CMakeLists.txt).

## Runtime y hardware

Windows x64/JVM21, loader NeoForge1.21.1, driver Vulkan capaz del efecto y del GL↔VK Win32 external-memory/semaphore de SR sobre el mismo adaptador. Revisar capacidades de storage/sampling/transfer por formato, subgroup/wave operaciones, features solicitadas por el SDK, colas/familias y memoria antes de activar. El manual dice Vulkan 1.x; no inventar una exigencia fija Vulkan1.3 o float16 sin revisar las permutaciones/dispositivo. Optical Flow usa wave operations y msad4; en hardware sin ejecución eficiente el rendimiento puede caer. [Optical Flow oficial](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/optical-flow/).

AMD y NVIDIA son objetivos del algoritmo sin requisito ML ni dependencia NVNGX/Reflex. La publicación inicial GPUOpen da rangos amplios y advierte contra FG por debajo de sus recomendaciones; no usar esas listas antiguas como garantía del driver actual. Plataforma conservadora de validación propuesta: RX5700/RDNA o superior, preferentemente RX6000+, y RTX20/Turing o superior, preferentemente RTX30+. Es un objetivo de ensayo; aceptación final basada en queries de capacidades y prueba GPU, no vendor ID ni compatibilidad declarada por este proyecto. [Hardware y requisitos AMD GPUOpen](https://gpuopen.com/news/fsr3-announce/).

AMD recomienda alrededor de 60 FPS reales antes de FG; FG no corrige latencia base ni el movimiento defectuoso del shader. No prometer x3/x4 ni rendimiento por tener outputs[4]. [Recomendaciones FSR3](https://gpuopen.com/fidelityfx-super-resolution-3/).

## Gates antes de dispatch real

- Completar distribución oficial local/pin/licencias y comprobar ruta directa sin proxy.
- Capturar delta de frame y metadatos de color/HDR/unidades; medir coherencia HUDless/color/depth/MV y frameID.
- Validar baseline runtime en instancia separada sin DH/SSRD; no tocar instalación real.
- Primer ensayo nativo futuro: query/create/destroy y validation layers, sin frames generados. Después prepare, single interpolation x2, UI/pacing, resize y stress AMD/NVIDIA por separado.

No está autorizado el backend completo en esta iteración. El contrato está en FSR_PROVIDER_CONTRACT.md; migración pendiente en UPSTREAM_MIGRATION.md.


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

