# Arquitectura Frame Generation - código actual, 2026-10-02

Análisis estático posterior al cierre de la baseline COMPILED de ambos mods. SR e365f5b32df88be8fb9925aaf0491658ebf4f705; Wisteria aeed1ef78e23ef0f8de51ad65d8804af17457f61. No constituye validación de GPU, SDK nativo, presentación real o estabilidad. No se implementó FSR FG.

## Correcciones frente a referencias antiguas

La API actual está bajo `io.homo.superresolution.api.registry.framegeneration`, no directamente api.registry como las clases del JAR de referencia 0.9.1-alpha.2. Los tipos actuales son FrameGenerationDispatchInput/Result, ProviderInputSnapshot y FrameGenerationProviderOutput. No asumir compatibilidad binaria con AsyncFrameGenerationDispatchRequest/Result antiguos.

No existe AsyncFrameGenerationScheduler.java en este checkout. Su responsabilidad está dividida entre AsyncFramePresenter, FrameGenerationWorker y PresentWorker. No transplantar el scheduler o native bridge del JAR antiguo a la API actual.

El comentario de FrameGenerationRegistry habla de orden de inserción para automático, pero la selección efectiva de startup y BackendNegotiator ordenan candidatos por prioridad descendente. El registro conserva orden y prohíbe IDs repetidos; no realiza por sí solo la negociación.

## Registro y selección

Archivos SR relativos a common/src/main/java/io/homo/superresolution/:

| Pieza | Responsabilidad verificada |
|---|---|
| api/event/FrameGenerationRegisterEvent | Evento público sin payload; proveedores llaman al registro estático |
| api/registry/framegeneration/FrameGenerationRegistry | Registro de descripciones; cache de Requirement, invalidateSupportCache/clearSupportCache; startup monohilo, sin thread safety |
| api/registry/framegeneration/FrameGenerationDescription | ID, grupo, prioridad, factory, Requirement, opciones, ejecución y binding de latencia |
| common/framegeneration/FrameGenerationDescriptions | Registro de entradas integradas/automáticas y publicación del evento |
| common/framegeneration/BackendNegotiator | Restricciones de grupo/backend y latencia; disponibilidad/dependencias del provider; prioridad descendente |
| common/framegeneration/FrameGeneration | Instancias, initialize/shutdown, gating, snapshot y despacho; declaración startup usada para preparar colas Vulkan |

Una descripción anuncia el modelo de ejecución antes de crear el dispositivo. La negociación runtime revalida isAvailable/isDependenciesSatisfied y que las capacidades async se hayan creado para ese provider. Cambiar provider con un presenter activo requiere drenarlo; cambiar carga del interposer puede exigir reiniciar.

## Dos modelos distintos

`APPLICATION_MANAGED_ASYNC`: SR captura en render thread, genera en worker FG y presenta en worker de presentación. El provider registra trabajo en los command buffers entregados; no adquiere imágenes swapchain ni llama vkQueuePresentKHR.

`EXTERNAL_INTERPOSER`: el provider configura el frame con prepareExternalFrame y finishExternalFrame. El interposer genera/presenta; SR no reserva imágenes adicionales para esas salidas.

FrameGenerationProvider define initialize, shutdownOnFrameGenerationThread, shutdown, isAvailable, supportedGeneratedFrameCount, presentationManagedGeneratedFrameCount, isDependenciesSatisfied, captureInputSnapshot, dispatchAsync, métodos externos y disable. **No define reset()/resize() como callbacks separados.** Reset aparece en constantes/snapshot; recreación corresponde a recursos/sesión según claves de tamaño/formato y ciclo del swapchain. initialize del provider async debe ser independiente del hilo; creación de sesión GPU pertenece al hilo FG.

## Captura y transferencia de recursos

FrameCaptureManager/CaptureFrameRing mantienen tres frames en vuelo. FrameResources contiene finalColor, hudlessColor, depth y motionVector, índice lógico y generación. FrameResourceLifecycle exige:

`REUSABLE -> RECORDING -> SEALED -> QUEUED -> DISPATCHING -> SUBMITTED -> REUSABLE`.

La ruta sin async puede pasar SEALED -> SUBMITTED. No reutilizar un slot aún owned. begin/destroy esperan la generación de submission del command buffer, completions de inputs DLSS-G y release de recursos propios. Una recuperación fallida marca el slot unrecoverable.

FrameTextureResource distingue copia propia de préstamo. En copia, importa memoria Vulkan a OpenGL, invierte Y (y componente Y de motion), signal ready GL->VK y espera release VK->GL antes de escribir otra vez. Profundidad copiada se representa como R32F. En préstamo conserva texturas y semáforos externos pero no destruye los recursos del propietario. FrameResources espera que la submission que publica release exista antes de permitir waits OpenGL para inputs prestados.

VulkanInterop/VkGlInteropSemaphore emplean memoria/semaphores externos y Win32 handles en Windows (FD en Linux); los handles exportados tienen cierre explícito según importación/plataforma. Compartir memoria no elimina las obligaciones de sincronización/layout/ownership.

## Scheduler actual y GPU

AsyncFramePresenter enlaza generationQueue y presentationQueue con capacidad limitada. Generation queue = MAX_IN_FLIGHT_FRAMES - 1 (2); máximo de cinco generados por real; presentation queue cuenta imágenes, con capacidad 6. El snapshot de provider se captura antes de transferir ownership; un ID/índice erróneo produce fallback real-only.

FrameGenerationWorker:

1. Marca el slot DISPATCHING y toma snapshot/configuración swapchain.
2. Adquiere command buffers de la command pool FG; al menos uno, uno por generado solicitado.
3. Llama FrameGeneration.dispatchAsync sin mantener el monitor global de FrameGeneration.
4. Valida resultado, output lease, conteo, tamaño/formato y que la lease no esté liberada. Fallos anteriores a submit permiten fallback real-only.
5. Submit separado por generado sobre requireFgQueue. Primera submission espera ready de captura; genera semáforos de handoff por imagen; última publica señales necesarias del real/release.
6. Publica batch completo: generados en orden y después real. Puede usar realOutput del provider o finalColor original.
7. Retira salidas en hilo FG **después** de presentationCompletions, gpuReadyFences y providerCompletion. Entonces output.release y reciclaje de semáforos.

Un error después de submit se maneja conservadoramente: slot unrecoverable y espera/limpieza de lo ya enviado; no se presenta como un simple fallo reversible antes de GPU.

PresentWorker posee adquisición swapchain, blit, presentación ordenada y sleeps de pacing. Descarta batches cuya configuración/generación swapchain ya no es actual. Prepara/submite imágenes antes del pacing para no retener la publicación de release de inputs detrás de intervalos de display.

VulkanDevice valida pertenencia del command pool y queue family; submissions usan submitLock por cola. La disponibilidad async requiere el dispositivo y capacidades vinculadas al provider startup. Handoffs GPU usan semáforos binarios; también existen timelines para seguimiento de submission/present y procesamiento de inputs por interposer. No intercambiar ambos contratos.

## Pacing, resize y shutdown

FramePacingEstimator observa tiempo de producción real, conteo planificado, generación swapchain y reset solicitado. FrameGenerationWorker calcula intervalo por imagen a partir del periodo/(generados+1), limitado entre 0.5 y 100 ms. PresentPacer/PresentWorker separan espera de GPU del instante de presentación; esto no garantiza visualmente ausencia de stutter sin mediciones.

VulkanSwapchain.recreate drena presentación, espera idle de la cola FG, recrea bajo swapchainLock y reanuda. Configuración conserva handle/generation para rechazar targets viejos. FrameGeneration shutdown coordina teardown afín al hilo FG con el presenter antes del shutdown independiente del hilo. Mantener esta secuencia en un futuro backend.

## Wisteria actual

Archivos bajo common/src/main/java/org/ireallywanttosleep/wisteria/backend/:

- WisteriaFrameGeneration registra `wisteria:ngx` (prioridad 200, APPLICATION_MANAGED_ASYNC) y `wisteria:streamline` (100, EXTERNAL_INTERPOSER), ambos DLSS_FG.
- NGX excluye binding Reflex Streamline; el backend declara dependencias sin obligación Reflex y SR proporciona pacing. NgxFrameGenerationAdapter verifica soporte/capabilities reales; no se eliminan checks de GPU/driver.
- NGX exige hilo FG para dispatch, usa final/hudless/depth/motion y constantes snapshot. FeatureKey cubre tamaños/formato; recreación exige que no queden output leases. OutputSlot/SlotLease conservan salidas hasta release. Si cambia output key mientras hay leases, devuelve fallo para real-only hasta drenar. Transiciones a GENERAL y manejo de layouts pertenecen al adapter.
- Streamline exige plataforma/native/session y binding Reflex Streamline. presentationManagedGeneratedFrameCount=0; el interposer presenta. Adapter valida token/índice/dimensiones, setConstants, tags depth/motion/hudless y opciones DLSS-G. finishExternalFrame recupera completion timeline del procesamiento de inputs y lo asocia a FrameResources.
- StreamlineReflexProvider emite markers PCL, configura opciones, respeta sesión/token y aplica tres frames de warmup antes de reflexSleep al invalidar pacing.

## Riesgos y evidencia pendiente

| Recurso | Garantía vista en código / trabajo pendiente |
|---|---|
| VkImage / VkImageView | Lease de salida, slots capture y destrucción diferida VulkanTexture; no retener raw handles más allá de ownership/generation |
| VkDescriptorSet | Ningún descriptor de un backend AMD fue creado o auditado. Diseñar pools/recreación y no afirmar validación de descriptors por haber compilado |
| VkCommandBuffer / VkFence | Buffers por dispatch, pool compatible con family y submission generation; esperar retirement antes de reset/destroy |
| VkSemaphore / timeline | ready/release/handoff y completions tienen owners distintos; no destruir ni reusar antes de consumo/retiro |
| Queue ownership / frames en vuelo | Cola FG validada y configuración/generación verificadas. Cambios de family y bridges nativos requieren revisión adicional por backend |
| Resize / shutdown | Drenaje, idle, claves de recursos y teardown afín al hilo; pendiente prueba real con validation layers y stress |

Este mapa cubre flujo/API y ownership visibles. No se descompiló masivamente la referencia, no se diseñó ni implementó un bridge AMD. Siguiente fase lógica: comparación dirigida de referencias con esta API actual; investigación oficial AMD después de esa comparación. Runtime mínimo sin DH/SSRD sigue pendiente y no se modificó la instalación real.


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

