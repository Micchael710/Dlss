# Contrato conceptual del provider FSR — 2026-10-02

## Implementación vigente: primer x2 PASS

La ruta nueva FsrFrameGenerationBackend/Adapter/Session/OutputPool y FidelityFxBridge
está implementada y ejecutada con SDK1.1.4/FSR FG3.1.4 Vulkan. API application-managed
async, sesión y pool en FG worker; inputs/sizes/timing/epochs inmutables. Outputs
0..5 en contenedor pero capability actual=1 y runtime únicamente x2. Preparar mayores
conteos no equivale a publicarlos como disponibles. Leases poseen generated y copia
real, y se liberan sólo tras completion GPU y PresentWorker. Ring lleno espera una
submission real antes de reutilizar; prueba de concurrencia aprobada.

4912 pares presentados, 4911 sin reset, cero nuevos errores en run final. Metadata
congelada en PresentImageBatch; NO_SWAPCHAIN_CONTEXT_NOTIFY evita notificaciones a
swapchain AMD. Sin WisteriaNative.dll/FsrNativeBridge/SPIR-V propio/scheduler viejo.
Binarios dentro de JAR, extracción/carga automática, OptiScaler OFF. Véanse
CURRENT_STATE.md, FSR_INPUTS_RUNTIME.md y logs/runtime/fsr-x2-result.json.

Upscaling/FG conservan registros independientes. Grupos FG: Off, AMD FSR FG y DLSS
FG. AMD anuncia un generado cuando bridge/API Vulkan están disponibles; contexto y
dispatch están comprobados en el dispositivo de esta baseline. Modos mayores se
deshabilitan por el límite real=1. NGX/Streamline mantienen sus requisitos de backend
y capabilities; no se habilitó una opción por el nombre RTX ni se sustituyó la
negociación existente. No se validaron nuevos backends de upscaling en esta fase.

Las secciones DESIGN_ONLY siguientes son el contrato histórico previo a la implementación.

DESIGN_ONLY / no implementación. API base actual en api.registry.framegeneration. SDK propuesto y prueba de viabilidad en AMD_FSR_FG_PLAN.md. Los siguientes requisitos son decisiones del proyecto, no garantías de funcionamiento AMD/SR.

Gate runtime 2026-10-02: **FAILED_RUNTIME_GATE** porque el JAR SR no incluye core native obligatorio. No se implementó el commit de metadatos ni se registró wisteria:fsr. Todos los MISSING de la matriz siguen pendientes. Primero restaurar una baseline runtime válida con source/provenance de core native, sin recurrir a WisteriaNative antigua. Ninguna compatibilidad nueva de API pública en esta iteración.

## Identidad y ejecución

ID propuesto `wisteria:fsr`. Búsqueda estática actual sin conflicto; el JAR antiguo sí lo registra, por lo que nunca cargar referencia antigua junto al provider nuevo. Validar duplicados runtime. Grupo propuesto `wisteria:fsr_fg` mediante BackendGroup.of y entrada automática conforme al patrón de registry; SR actual solo predefine DLSS_FG. Prioridad propuesta 150, dentro de ese grupo; no pretende competir por prioridad con el grupo DLSS. Registrar por FrameGenerationRegisterEvent; backend/Requirement verifica DLL/ABI/dispositivo, sin falsear disponibilidad.

APPLICATION_MANAGED_ASYNC candidato: patrón NgxFrameGenerationBackend/Adapter para lifecycle, snapshots, slots y leases. Excluir binding Reflex Streamline/interposer inicialmente; no copiar NVNGX. PresentWorker conserva acquire/blit/present/pacing; provider solo graba commands. Un generado por real (x2 total) inicialmente. Primera captura/reset puede ser real-only mientras se siembra historial, sin afirmar que SDK ya entregó un frame.

## Matriz de inputs

La referencia shader rgba16f/autotex no fija el formato runtime de FrameResources. Las texturas Vulkan capturadas exponen su formato real, tamaño y generation: inspeccionar cada frame y rechazar incoherencias. La tabla expresa disponibilidad del contrato actual, no calidad validada de las entradas. Correspondencia oficial de prepare/dispatch: [header FFX 1.1.4](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v1.1.4/ffx-api/include/ffx_api/ffx_framegeneration.h); recursos de bajo nivel: [Frame Interpolation](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/frame-interpolation/).

| AMD INPUT | ¿EXISTE EN SR? | FORMATO ACTUAL | FORMATO AMD | CONVERSIÓN NECESARIA | RESPONSABLE PROPUESTO |
|---|---|---|---|---|---|
| Current/present color | Sí finalColor | VkImage, formato runtime | FfxApiResource color display-size con formato soportado | Mapear VkFormat y color space; MISSING metadatos completos | SR captura; adapter valida |
| Previous color | No input explícito persistente | Slot capture reusable | Historial de interpolación interno por sesión | No retener el raw image anterior; SDK history / copia propia si necesaria | Contexto AMD; adapter controla sesión |
| HUD-less color | Sí hudlessColor | VkImage formato runtime | HUDLessColor display-size, formato declarado | Validar alineación/tamaño/color con finalColor | SR captura; adapter valida |
| Depth | Sí | Copia R32F; préstamo puede diferir | Depth float32 render-size y flags inverted/infinite | Mapear formato y convención; no convertir a lineal por intuición | Adapter, SR metadata |
| Motion vectors | Sí, calidad parcial de referencia | VkImage según fuente; escala y jitter flags en constantes | 2 componentes, render-size por defecto o flag display-size | Validar signo/Y, escala a unidades esperadas, canales/flags; MISSING evidencia entidades | Adapter; shader productor futura fase |
| Render size | Dimensiones de depth/MV disponibles | Enteros por textura | Dimensions2D real render size | MISSING declaración conjunta coherente; no usar output size por defecto | SR snapshot/adapter |
| Display size | Sí DispatchInput y output key | width/height int | Dimensions2D | Comparar con color/swapchain y generation | Adapter |
| Frame delta time | MISSING en ProviderInputSnapshot/constantes | Pacing estimator no es input FG de captura | float milisegundos desde frame real anterior | MISSING | SR futura captura temporal |
| Jitter | Sí constantes | float X/Y | Subpixel jitterOffset | Verificar unidades render-pixels y eje tras interop | Adapter |
| Camera | Sí near/far/FOV/pos/basis/matrices | float / arrays inmutables | near/far/FOV radianes + linked CameraInfo | Validar normalización, handedness y FOV; MISSING viewSpaceToMetersFactor explícito | SR metadata; adapter |
| Reset | Sí snapshot/constantes | boolean/byte | dispatch.reset + historial/frameID | No confiar en unused_reset de prepare; propagar discontinuidad al dispatch | Adapter |
| Exposure | MISSING input FG explícito | Shader JSON auto no proporciona valor snapshot | No exposure obligatorio en FG prepare/dispatch de este tag; upscale lo maneja aparte | No inventar exposure=1; si pipeline pre-expuesto, MISSING semántica | SR metadata futura |
| Transfer function / HDR luminance | MISSING snapshot completo | VkFormat no define transfer function | enum transferFunction, minMaxLuminance | MISSING | SR metadata/color contract |
| Optical flow vector / SCD | MISSING en SR | Ninguno | Internos SDK; lowlevel RG16_SINT por bloques8 y R32_UINT 3x1 SCD | No generar con MV del juego ni reutilizar shader custom | Contexto AMD |
| Generated output | API existe; imágenes FSR MISSING | FrameGenerationProviderOutput / lease | FfxApiResource output display-size soportado | Crear imágenes propias storage/transfer compatibles, mapear formato | Adapter pool propio |
| UI | Final y hudless sí; UI RGBA separada MISSING | No recurso UI independiente en FrameResources | HUDless detection o superficie UI/callback | Ruta HUDless candidata; no restar colores para inventar alfa | Adapter/AMD; futura captura SR si cambia modelo |
| frameID / generation | Índice lógico y generation sí | long | uint64 frameID coherente prepare/configure/dispatch | Mapear por sesión; hueco/reorder implica reset; nunca reusar índice viejo | Adapter |

Optical flow es distinto de los motion vectors del juego. Formatos y dimensiones de OF descritos en [manual AMD OF](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/optical-flow/). Exposición y UI MISSING no son automáticamente bloqueos si la ruta FG/HUDless seleccionada no los exige; color transfer/HDR sí necesita semántica explícita.

## Outputs y lifetime

Result.failed(reason) antes de submit: ninguna lease publicada y ningún trabajo GPU fuera de command buffers de SR. Success: exactamente una imagen generada, OutputKey width/height/format correcto, completion válida, lease no liberada. RealOutput opcional solo si el backend realmente produce una imagen real propia; usar finalColor de SR en otro caso. UNCHANGED/RESET/SEEDED deben describir historial real, no presentar éxito artificial al sembrar.

Pool acotado por frames en vuelo y leases; si agotado devolver real-only, nunca reciclar imagen todavía usada. No publicar outputs parciales. Lease abort antes de GPU; release idempotente en hilo FG solo tras todos los presentationCompletions, gpuReadyFences y providerCompletion. La sesión vieja permanece viva mientras cualquiera de esos usos exista. SDK puede escribir su historia al siguiente frame: comprobar buffering interno/direct path antes de permitir varios dispatch en vuelo sobre un mismo contexto.

## Thread affinity y resource ownership

| Recurso / propietario | Create | Use | Destroy | Lifetime y sincronización |
|---|---|---|---|---|
| FidelityFX context y scratch CPU — bridge/session | FG thread | FG thread serial | FG thread tras drain/GPU completion | Por SessionKey; no acceso concurrente render/present |
| OF/history/intermediate VkImages — contexto AMD o allocator bridge | FG | FG commands | FG via SDK después de idle | Sesión; jamás alias con captura prestada |
| ImageViews internas — mismo owner de imagen | FG | Descriptors FG | FG antes de imagen, tras todos sus usos | No cache de vistas de input entre generations |
| Descriptor pools/sets/pipelines — backend AMD | FG | Commands SDK | FG tras completions | No reset/free de set/pool en vuelo; confirmar disciplina SDK |
| Generated VkImages/views — adapter output pool | FG | FG escritura / PresentWorker lectura | FG tras lease retirement | OutputKey + session generation; layouts tracked |
| Output leases — adapter | FG dispatch | Transferidas a batch; PresentWorker consume | FG release/abort | Release no significa destruir inmediatamente imágenes reutilizables |
| Inputs capture — SR/CaptureFrameRing | Render/SR | FG préstamo durante dispatch | SR según lifecycle | Jamás destroy/free por bridge; no retain raw handle para próximo frame |
| Command buffers/pools/fences — SR worker/device | SR FG | Bridge graba, SR submit | SR | Bridge no end/reset/free/submit sobre ellos |
| Ready/release/handoff semaphores — SR interop/workers | SR | GL→FG→Present→GL | SR tras consumo | Bridge no destruir ni sustituir su contrato |
| VkDevice/queue/swapchain — SR | SR | Bridge consulta device; SR queues/present | SR | No ownership nativo ni vkQueuePresentKHR del provider |

initialize solamente estado CPU/capability, sin asumir hilo GPU. captureInputSnapshot en render thread: copiar números/arrays; sin raw views persistentes. dispatchAsync, create/recreate/retire y shutdownOnFrameGenerationThread en FG. shutdown final solo estado CPU/biblioteca cuando ya no existen sesiones ni callbacks. No serializar GPU con un lock Java como sustituto de semáforos.

## Sincronización propuesta

1. SR captura GL y publica ready externo. Primer submit FG espera ready antes de leer input. External memory Win32 comparte memoria; external semaphore ordena acceso. Mantener cierre de handles Win32 conforme a VulkanInterop actual.
2. Convertir FfxApiResource desde imagen, formato, tamaño, usage/layout y generation validados del frame. Image barrier establece access/stage/layout reales; actualizar tracker de Java únicamente conforme a commands grabados/submitted. Restaurar un número de layout Java sin barrier no restaura GPU.
3. Prepare y interpolation ordenados en command buffer FG compatible con pool/family. Depender de ready, registrar barriers de compute writes→reads→transfer. Adapter no submite ocultamente. SR submitLock protege host access de queue; un mutex nativo por contexto protege FFX CPU, no finalización GPU.
4. Handoff binario FG→PresentWorker protege output antes del blit. Si familias distintas y recurso EXCLUSIVE, release/acquire ownership explícitos; no asumir que GENERAL o el semaphore transfieren ownership. Si concurrent sharing, declarar familias exactas y mantener barriers.
5. SR publica release inputs VK→GL después de su último uso. Si SDK retiene entradas beyond submit, copiar a recursos propios o extender explícitamente contrato de completion SR antes de habilitar; nunca seguir leyendo un slot REUSABLE.
6. No reciclar fences, timelines, binary semaphores, descriptor sets ni command buffers hasta completions de sus usos. Timeline values asociados a generation, sin tratar contador como fence binario. Output retirement espera presentación, GPU y provider. Teardown espera submissions/callbacks/leases y destruye vistas/descriptors antes de memoria.

## Session lifetime / reset / resize

SessionKey propuesta: device identity/generation, FG queue family, max render size, display size, input/output/HUDless formats, depth conventions, MV flags, transfer function/HDR mode, provider id/config. Key de imágenes salida: display width/height/format/count + session epoch. Swapchain generation invalida batches, no debe confundirse con capture generation. Snapshot actual no expone todas estas claves: obtener solo por interfaces reales o extender snapshot, nunca inventar getter.

| Evento | Decisión propuesta |
|---|---|
| Window resize | Drenar batches/leases, esperar FG, recrear si display/key cambia; reset/seed |
| Fullscreen toggle | Tratar swapchain generation como discontinuidad; recreate si formato/tamaño cambia |
| Render resolution cambia | Revalidar depth/MV; dentro de max permitido reset prepare/history; fuera de max recreate tras drain |
| Shader reload | Invalidar referencias/descriptors, reset; nueva sesión si formatos/semántica cambian |
| Cambio mundo | Snapshot reset/epoch; no extrapolar cámara antigua; seed real-only |
| Teleport/camera cut | Reset de dispatch/history, no destruir sesión si key igual |
| HDR/formato cambia | Nueva SessionKey y output pool tras drain; no reinterpretar VkFormat/color transfer viejo |
| Cambio provider | Cerrar admisión, drenar worker/leases, teardown FG, shutdown; negociar capacidades startup |
| Swapchain recreation | Usar drenaje VulkanSwapchain actual; descartar batches viejos, reset y validar key |
| Salto frameID / dropped frame | Reset historial con siguiente real válido; prepare/configure/dispatch mismo ID |

No añadir callbacks reset()/resize() imaginarios al contrato upstream. Resize se detecta por input/session key; eventos semánticos necesitan llegar al snapshot real con reset/epoch.

## Failure y fallback

DLL/ABI/capabilities ausentes: provider unavailable y causa observable, nunca quitar checks GPU. Datos incoherentes/handle0/slot ocupado/recreate pendiente antes de submit: Result.failed y real-only. Error de registro de commands: abort solo recursos no enviados; SR maneja buffers. Error después de submit/device lost: detener admisión y reutilización, marcar unrecoverable; retiro conservador vía SR, no destruir recursos en vuelo ni continuar como si el error fuese pre-submit. Diagnóstico incluye provider, SDK/ABI, session epoch, frameID, formats, queue families y fase de fallo sin datos personales.

## Native boundary / funciones requeridas propuestas

Funciones siguientes son una propuesta ABI propia; **no existen aún**:

- queryBridgeAbi/querySdkVersion/queryCapabilities(device): datos/versiones reales y razones de rechazo.
- createSession(device, physicalDevice, queue metadata, SessionKey): token opaco validado; sin ownership de VkDevice.
- configureFrame(session, copied snapshot, HUDless descriptor): correspondencia configure del tag y flags, sin proxy no verificado.
- recordPrepare(session, borrowed command buffer, depth/MV descriptors, frame metadata).
- recordGenerate(session, borrowed command buffer, color/HUDless, owned output descriptors): FFX dispatch, un generado.
- querySessionDiagnostics/queryMemoryUsage: estado y errores; ninguna capacidad inventada.
- destroySession(session): solamente tras drain y GPU completion; doble destroy/token stale rechazados.

No native reset global/setAlgorithm/setMultiplier heredados. Reset via descripción por frame o recreate. FFX entrypoints usados serán create/destroy/query/configure/dispatch oficiales, structs con type/pNext exactos. No mantener pointers JNI de arrays liberados ni JNIEnv de otro hilo. Preferir tabla de tokens con generation a exponer direcciones C++ como IDs.

## Validación futura y primer cambio

Primer commit de código recomendado: `fg: capture immutable frame timing and color metadata`. Capturar timestamp/delta real en productor y metadatos con procedencia explícita, sin provider disponible ni native dispatch. Validar snapshots no mutables, unidades y discontinuidades; no usar pacing interval como delta de simulación. Después seleccionar DLL exacta y demostrar configure/direct dispatch sin apropiación del swapchain. Baseline runtime y pruebas validation-layer/resize/world/provider/stress deben pasar separadamente antes de habilitar FG.


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

