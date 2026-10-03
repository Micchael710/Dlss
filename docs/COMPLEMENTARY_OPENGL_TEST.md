# Complementary OpenGL — 2026-10-02

Resultado **FAILED_VISUAL_GATE**, sin crash observado. La interrupción anterior por Escape fue de Computer Use y no un fallo del mod. Esta iteración sí produjo evidencia de un problema gráfico con el pack activo. Baseline OpenGL sin shader y core nativo permanecen válidos; no se reconstruyó nada. No se probaron Vulkan, resize ni fullscreen al encontrar el fallo.

## Entorno y artefactos conservados

Misma runtime-baseline/instance, Minecraft1.21.1/NeoForge21.1.219/Java21, Iris1.8.8+mc1.21.1, Sodium0.6.13, NVIDIA RTX3050Ti Laptop/OpenGL4.6/driver596.49. SR presentation/backend=OPENGL, skip_init_vulkan=true, upscale_algo=fsr1, upscale_ratio=1.7, FG OFF. FSR1 upscale existente no es implementación FSR Frame Generation.

JAR SR SHA256 51c22ce1517002ade6bd891e1d104fed50348e8ffc28dc2f5cc8f557482593e0; core DLL80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec, ambos revalididados sin cambio. Commit JNI03c0e447d37a8369f5e8864199bd971ec3a03f4b. No cambios nuevos en los repositorios fuente ni push/PR/fork/migración.

Original Complementary-SuperResolution-Wisteria.zip y copia en instance/shaderpacks tienen SHA256680e76ee11b0513c84602c3a3e151c64c236c5bc952fff4492265b8d4a4cf7d7. Original y .minecraft real intactos. Cliente anterior estaba cerrado; se relanzó solo la instancia aislada. Usuario movió la nueva ventana a pantalla secundaria; captura correcta de Minecraft antes de cualquier clic. No se actuó sobre otras aplicaciones.

## Activación e integración runtime

Iris UI: Options → Video Settings → Shader Packs, pack exacto seleccionado, Apply; nombre amarillo/activo. Config iris.properties: enableShaders=true y shaderPack=Complementary-SuperResolution-Wisteria.zip. Log16:47:15.963: Using shaderpack; perfil HIGH, 0 opciones cambiadas por usuario.

SR16:47:14.844: **Loading shader-pack interface configuration: superresolution.v1.json**.16:47:14.880: Shader pack shaders supports super resolution. El archivo versionado tiene prioridad sobre superresolution.json, como establece ShaderCompatHandler.loadConfig. ZIP contiene ambos archivos en root y shaders/; el versionado leído contiene los mismos bindings: color autotex3, depth depthtex, motion_vectors autotex2, BEFORE composite7, input region[0,0,-1,-1], output target autotex3 region[0,0,-2,-2], internal_format rgba16f, jitter enabled, hdr true, auto_exposure true. Estos son datos del pack/procesamiento, no metadatos nuevos de FG.

Config procesada y mundo cargado no prueban individualmente la resolución real de cada textura ni que el dispatch BEFORE composite7 haya ocurrido. No hay log específico de handles/formatos/pass ejecutado; ese nivel queda **NOT_INDIVIDUALLY_VERIFIED_RUNTIME**. No se registraron errores explícitos de parsing de la interfaz SR o color/depth/motion bindings, pero su ausencia no certifica corrección.

Mundo de prueba existente, sin crear otro: servidor integrado16:49:01 confirma entrada. Pipeline Iris creado; chunks y shaders terminaron carga hacia16:49:27. Capturas posteriores confirman agua/reflejos/terreno iluminados por shader y fallo persistente; no es solamente una pantalla de carga.

## Fallo visible y alcance

Escena restringida a la región inferior izquierda, aproximadamente502×282 dentro del área cliente854×480. Parte superior y derecha negras; HUD/hotbar conserva ancho de pantalla. Mano y crosshair quedan desalineados respecto al centro de la escena reducida. La proporción es compatible con854/1.7 y480/1.7; son estimaciones visuales de pixels, **no mediciones de attachments GL**. Evidencia: logs/runtime/complementary-opengl-world-failure.jpg.

| Comprobación | Resultado |
|---|---|
| Pack detectado/seleccionado/activo | YES, UI/config/log/cambio visual |
| Mundo con shader | YES, servidor y captura |
| Terreno/bloques | Visibles en región reducida, composición FALLIDA |
| Mano | Visible dentro de escena reducida; no prueba de objeto sostenido |
| Cielo/nubes | Visibles en región reducida; inspección completa NO |
| Agua/reflejos | Visibles en región reducida; calidad completa NO VALIDADA |
| HUD | Visible a tamaño de display, desalineación con escena |
| Vegetación/entidades/jugador/partículas/inventario | NO PROBADOS |
| Movimiento lateral/giro lento/rápido/ghosting/trailing/jitter/flickering | NO PROBADOS tras fallo |
| Sol/luna/transparencias/depth/motion vectors | NO VALIDACIÓN individual |
| Clipping/bordes | Fallo visible: escena parcial, negro arriba/derecha |
| Resize/fullscreen | NO EJECUTADOS por gate fallido |
| Crash | Ninguno observado al exportar evidencia; no validación de estabilidad |

COMPLEMENTARY LOADED=YES; VISUALLY_VALIDATED=NO; GENERATED FRAMES=NO; STRESS TESTED=NO. No se interpreta carga exitosa como validación gráfica.

## Logs y diagnóstico sin corregir

OpenGLDebug16:49:28.134 registra id1280, API/type=ERROR/severity=HIGH: **GL_INVALID_ENUM error generated. Internal format not supported.** Aparece otra vez16:53:29.440. El registro usa nivel INFO aunque describe un error GL; exportador lo incluye. No identifica llamada, textura ni framebuffer: no atribuirlo automáticamente a rgba16f ni afirmar que causa el rectángulo negro. No error fatal/hs_err/crash-report observado en esta prueba.

Advertencias Iris: inPaleGarden no resuelve BIOME_PALE_GARDEN; inSulfurCaves no resuelve BIOME_SULFUR_CAVES; endFlashFactor0 no resuelve endFlashIntensity, derivados endFlashFactor1/endFlashIntensityM rotos. shaders.properties del pack referencia esos identificadores. También block.10104 usa propiedad variant inexistente en minecraft:stone_slab. Son problemas de compatibilidad del pack/entorno documentados; no se modificaron ni se confirmó que expliquen el fallo principal.

Hipótesis de diagnóstico **INFERENCE**, pendiente de runtime: shaders/lib/common.glsl552–558 define SR_ACTIVE cuando SR_ENABLE=1 y fuerza FXAA_DEFINE=-1; shaders.properties104–108 deshabilita composite7 si FXAA_DEFINE==-1 o FXAA_STRENGTH==-1. El trigger de SR apunta precisamente BEFORE composite7. Falta demostrar cómo resuelve Iris esa condición en su preprocesador de propiedades/perfil HIGH y si composite7 existe/se ejecuta en este pipeline. No convertir estas dos condiciones estáticas en causalidad confirmada.

Otra línea pendiente: correspondencia entre dimensiones del render/display, viewport final y colortex/autotex en el copy/output de SR. TextureRegion/ShaderCompatTextureInfo confirman que -1 significa render size y -2 display size; no se ha probado el tamaño real de esas texturas. El error GL de formato requiere traza de llamada o instrumentación autorizada posterior para identificar la operación exacta.

Logs separados: complementary-opengl-client.log (sesión actual), complementary-opengl-game.log (snapshot saneado), complementary-opengl-client-sanitized-snapshot.log y complementary-opengl-errors.log (incluye advertencias/trazas relevantes y errores GL). Resultado estructurado complementary-opengl-result.json; config efectiva del ZIP complementary-runtime-selected-config.txt. La sesión anterior interrumpida se conserva en complementary-opengl-previous-interrupted.log y no acredita este resultado.

Siguiente paso: aislar ejecución de composite7/dispatch y revisar tamaños/viewport/formato de attachments en el pipeline activo. No corregir todavía, no cambiar ratio para ocultar el problema, no habilitar Vulkan ni FidelityFX. Cliente queda abierto con la escena fallida para inspección; no se continúa con pruebas gráficas posteriores.


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

