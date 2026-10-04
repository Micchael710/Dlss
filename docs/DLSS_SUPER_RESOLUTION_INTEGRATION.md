# Integración NVIDIA DLSS Super Resolution

## Handoff combinado — 2026-10-04, posterior a b3dd23c

Standalone PASS histórico no repetido. Una única prueba SR+DLSS-G x2
`20261004-181754-546-handoff-x2` alcanza FG Create1/Evaluate2, pero falla
con VK_ERROR_DEVICE_LOST en el segundo submission input del consumer.
SR Evaluate4/outputQueued4; no events de completion SR antes del crash.
No G1 presentado, combinación FAIL y shutdown no normal. La implementación
de receipt/señal post-SR y copia persistente supera INPUT_MAPPING, pero
NO se declara segura/validada en GPU. Post-NGX layout efectivo y causa del
device lost quedan UNPROVEN. Se mantiene matrix guard; advertencia inicial
Infinity no bloqueó los siguientes dos Evaluate. No retry ni investigación
MFG/AMD. Config restaurado byte-for-byte. Contrato y evidencia detallados
en [DLSS_SR_DLSSG_X2_INTEROP.md](DLSS_SR_DLSSG_X2_INTEROP.md).

## Iteración Vulkan y teardown — 2026-10-04

Desde eaca5f4 limpio. La opción existente dlss activa la infraestructura Vulkan existente durante init global, incluso con skip_init_vulkan=true; init es idempotente y no cambia el caso FSR/OpenGL con skip. Switching a DLSS con Vulkan ausente queda unavailable por gate real, sin init tardío ni device duplicado. El selector revisa device, extensiones GL de memoria/semaphore Win32 y disponibilidad real NGX fuera del cache de requisitos de plataforma. El provider comprueba estos prerequisitos antes de crear recursos.

InteropTeardown drena sólo cuando hay recursos creados, después libera command ring, feature y recursos compartidos. Una falla mantiene los owners alcanzables. El failure path previo ya no accede a Vulkan null ni hace glFinish sin recursos. NgxInitializer no llama Shutdown forzado tras Init fallido; JNI guarda el device sólo tras éxito real, rechaza Init con handles nulos y llama Shutdown1 sobre el device inicializado. Release fallido conserva el owner; wrappers cerrados se ponen null. La destrucción del contexto oculto se difiere mientras exista el renderHandle aunque la presentación efectiva haya sido desactivada. No se atribuye el crash histórico 0xC0000409 a una causa probada.

Build C++ y NeoForge Java21 PASS. Tests mínimos: presets, política skip/DLSS/FSR, gate Vulkan ausente, cleanup de owners creados/no creados y fallo inyectado; smoke JNI real de Shutdown antes de Init, Init con handles Vulkan nulos rechazado, Shutdown posterior y Create con handles nulos rechazado/double-close seguro. Estos son checks CPU/JNI, no pruebas GPU de Init/Create fallidos. Wisteria se reutiliza sin cambios desde su build anterior; nueve clases/dos DLL AMD y clases FG/presentation siguen byte-identical, sin ejecución AMD gráfica.

**Standalone PASS**, 20261004-173230-747, Java cliente25.0.4, FG OFF. Vulkan device/GL interop true, native loaded, Init1, requirement supportMask0, SuperSampling.Available1 y FeatureInitResult1, Create1/handle válido. Flags67: HDR, low-res motion y autoexposición, depth no invertido/motion no jittered. Tres creaciones exitosas por ciclo menú/mundo/layout, no retry de falla. Evaluate4421, completion4421, output queued4421. Render495x278 -> output854x480, Balanced enum1. Depth compartidoR32F, motionRG16F, jitter real variable y scales495/278, preExposure1. La validación de depth/motion es de recursos/flags/escala; no se muestreó su distribución numérica ni ground truth.

Output VkImage1968400148752 corresponde a GL93; el dispatcher registra SR_output93 y target autotex3 GL66 a854x480. Copia al target con dispatch true y composite7 usa viewport854x480/FBO completo. Usuario respondió lista al check de pantalla completa/HUD/sin negro. Readbacks30–32 después de cargar mundo: cero fracción RGB negro, tres hashes diferentes; MAE frente a input nearest3.3134/4.0526/3.4717. Imagen diagnóstica inspeccionada: mundo completo, no esquina reducida. Es evidencia funcional acotada, no evaluación exhaustiva de calidad temporal. CPU frame transport sigue0; sólo tres pares de readback diagnósticos.

Shutdown: drain/feature release -> NGX shutdown1 -> features retired -> contexto oculto/backend graphics destroyed -> proceso0. No crash/device lost; 0xC0000409 no reproducido. No resize de estrés.

**Combinado FAIL**, único run20261004-173835-015-x2, mismo SR/Wisteria, FG X2/count1. DLSS SR sigue real: Evaluate10492/completion10492, cierre0. Sidecar FG inicializa, pero el blocker decisivo es INPUT_MAPPING: DlssgFrameGenerationAdapter rechaza frame.hasBorrowedAlgorithmInputs(), con error explícito de que los inputs del upscaler Vulkan necesitan un queue join auditado. integration-failure.txt y DLSSG_EXPERIMENT_FAIL lo confirman. El plugin registra Create0/Evaluate0 y clean exit; frame-sequence sólo contiene SESSION, provider-events vacío: generated presents0. La negociación inicial fg=wisteria:dlssg pasa luego a fg=null. También se observó FGConstantsBuilder cameraViewToClip no invertible; es advertencia adicional, no la causa demostrada del disable. Confianza alta en el guard INPUT_MAPPING; origen de la matriz pendiente. STOP sin nuevo intento, sin retirar guard ni cambiar FG.

Orden de fuente comprobado: NGX SR -> copia output a autotex3 -> composite7 y siguientes -> captura display HUDless antes de GUI/final tras GUI -> FG D3D12 sidecar -> PresentWorker Vulkan. Depth/motion se prestan desde inputs render R32F/RG16F del algoritmo. Por tanto FG recibe color display posterior a SR, no su color render previo. En el combinado no se validaron tamaños dentro de Evaluate FG porque no se alcanzó. PresentWorker sigue sole present owner, clases de presentación y FG intactas, sin Present D3D12 ni VkQueuePresent en provider. El x2 histórico no se convierte en PASS de esta combinación.

Configuración aislada restaurada byte-for-byte al snapshot inicial, SHAade8502bc8d5d523397b5e1838e1f9100d9782ff116c12ef7484594095172462; config-restoration.json registra comparación. El nuevo policy de startup DLSS resuelve Vulkan incluso con el skip del snapshot. Native propio de esta fase SHA5704e78307ad2eed47648791e5a04739efe16fa16b67bd4f78b59fd4dda1595f. Runtime NVIDIA y componente comunitario quedan locales/ignorados.

MFG histórico intacto: G2_IMAGE_GENERATION=PASS_BOUNDED_DIAGNOSTIC, optional statusUNKNOWN_0xffffffff, x3BLOCKED_STATUS_SEMANTICS_UNPROVEN. Sin presentación G2 ni x3/x4/x5/x6 nuevos. Próximo bloqueo concreto: queue join seguro para inputs prestados SR Vulkan -> FG; revisar además productor de proyección. Sin nueva investigación MFG en esta fase.

## Historia preservada de la primera fase SR

Inicio de esta iteración: `5da09874956956b04a5f413999a5c4217235c6a2`, main limpio. MFG no se volvió a probar. La opción existente `dlss`, nombre NVIDIA DLSS, pertenece al AlgorithmRegistry y usa `io.homo.superresolution.common.upscale.algo.dlss.DLSS`. No se agregó otra entrada.

## Error inicial demostrado

El build core configuraba SR_NGX=OFF y el checkout local del SDK bajo SRNativeNGX/third_party/DLSS estaba vacío. El JAR anterior no contenía `lib/libSuperResolutionNGX+win64+release.dll`. El log del cliente de la prueba MFG anterior registra que la extracción opcional del recurso falló porque no existía. Una carga real desde Java25 del path esperado reprodujo UnsatisfiedLinkError: Can't load library. NgxInitializer ocultaba ese error devolviendo false; DLSS reintentaba una vez y terminaba con NGX unavailable. La excepción de selección se deduce de esa ruta de código; no se abrió otra sesión de Minecraft para reproducir la UI inicial.

DLSS_SR_INITIAL_FAIL_STAGE=NATIVE_RESOURCE_EXTRACTION_AND_LOAD. NGX Init/Create/Evaluate no se alcanzan sin el binding. No se atribuye el fallo original a la RTX3050Ti o al runtime NVIDIA.

## Implementación y build

Se reutiliza la ruta oficial NGX Vulkan que ya existe, no un sidecar D3D12. SR_NGX_LIB ahora puede construirse de forma independiente con headers públicos y nvsdk_ngx_s.lib oficiales que ya estaban verificados localmente, sin reconstruir AMD/core. `tools/build_sr_ngx.ps1` produce `libSuperResolutionNGX+win64+release.dll`. La propiedad de build NeoForge `sr_ngx_native_dir` incorpora esa DLL propia al recurso lib/ esperado por NativeLibManager. SHA256: d471331c34a695b958ed1d797c066ea4e95ddfd5b0996575ad41332536d33652.

El runtime DLSS SR oficial se descargó de NVIDIA/DLSS commit374959484e79a640feaba44c93ac8cfb0a03f5b5, versión310.9.1.0; firma Authenticode válida de NVIDIA. Hash3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983. Se conserva local y no se publica en Git. No se usa el componente comunitario DLSS-G para SR.

Se consulta SuperSampling.Available realmente antes de crear la feature, se registra FeatureInitResult, y se exige handle válido. Evaluate fallido o contexto ausente produce error explícito y no devuelve output no generado. Se eliminó el retry automático. Los presets existentes de UI UltraPerformance/Performance/Balanced/Quality/DLAA se traducen al enum público NGX según ratio y dimensiones reales; no se inventan capabilities. La prueba CPU cubre presets, redondeo e inputs de tamaño inválidos.

Diagnóstico opt-in `sr.dlss.runDir`: Java observado desde el cliente, load, Init, requirements, capability, Create, Evaluate, completion y output que se encola. Las completions se registran después del wait ya existente del command ring o del drain de recursos antes de destruir la feature. No se añade un wait CPU por frame entre APIs. Tres readbacks de color/output mediante GL, ordenados después de la espera GPU al semáforo Vulkan, sirven sólo para observación.

## Inputs reales de la prueba

El shaderpack Complementary-SuperResolution-Wisteria.zip existente tiene superresolution.json: jitter activado, upscale antes de composite7, color autotex3, depth depthtex, motion autotex2, salida autotex3 a resolución display. Declara RGBA16F/HDR, auto exposure=true y motion_jittered=false. No se modifica el shaderpack ni se inventa exposure texture: se usa la autoexposición soportada por NGX y el preExposure del dispatch.

Depth se convierte al recurso R32F de render resolution, con range normalizado del depth real. El flag de inversión proviene del estado del provider; en Iris1.8 la detección de DepthTransformer debe verificarse con el evento CREATE_INPUTS antes de declarar la convención del run.

Motion viene de CalculateMotionVector: previous coordinate minus current coordinate, multiplicado por upscaleRatio para expresarlo en coordenadas normalizadas del render. NGX recibe scaleX=renderWidth, scaleY=renderHeight para convertirlo a píxeles. Sky usa sólo rotación; mano tiene motion0. Son limitaciones de calidad del productor existente, no nuevos inputs. La prueba usa flip_vk_gl_interop_resources_y=false; no se altera la convención de memoria compartida GL/Vulkan. El camino flipY existente invierte imagen y signo Y de motion; no se valida ese camino en esta prueba.

Jitter viene de DispatchResource/FrameData y usa píxeles render, como exige el [helper oficial Vulkan](https://raw.githubusercontent.com/NVIDIA/DLSS/main/include/nvsdk_ngx_helpers_vk.h). El pipeline existente calcula secuencia Halton/phase count para el algoritmo con jitter; usar esa utilidad matemática no ejecuta FSR. Reset procede de consumeHistoryReset al crear/recrear contexto. Los tamaños render/display se registran desde cada Evaluate.

## Validaciones previas al cliente

Binding propio Windowsx64 compilado. Carga real JNI y rechazo de handles nulos pasaron, sin simular GPU. El primer build falló por el gestor PDB de MSVC; /Z7 permitió compilar. El primer build Java configuró Fabric y falló por dependencia ajena; loader=neoforge lo aisló, y se descargó una dependencia LWJGL faltante. Build NeoForge completo pasó con bytecode21. El paquete experimental verifica native exacto, nueve clases/dos DLL AMD byte-identical, y clases framegeneration/presentation intactas. El JAR MFG previo sigue conservado con hashdab4218545c560b7ba242560fac53e1fb93ad62c2127e8d227a5bade77cc2e65.

## Runtime: FAIL antes de NGX

El arranque preparado `20261004-162909-911` falló antes de seleccionar DLSS: Path.write_text tradujo saltos Windows ya presentes y duplicó CR; NightConfig rechazó el TOML y regeneró defaults FSR1/OPENGL. No hubo NGX Init/Create/Evaluate. El cliente se cerró; el usuario pidió continuar. Se conserva client-config-parse-failure.log/setup-result.json y se corrige el preparador normalizando CR y escribiendo bytes. También se exige presentación VULKAN explícita. No se atribuye este error del preparador al backend DLSS.

Prueba efectiva `20261004-170642-750`: TOML parseado/validado antes de lanzar; DLSS existente, preset Balanced ratio1.724, FG OFF, Vulkan, Java25.0.4, Minecraft1.21.1/NeoForge21.1.219, instancia aislada. No se declara PASS por build, load ni Init. Debe comprobarse el output y el mundo/HUD antes de cualquier resize o segunda sesión DLSS SR + DLSS-G x2. Si falla DLSS, no se repite automáticamente ni se activa FG.

PresentWorker continúa como presentador final Vulkan. DLSS SR sólo escribe output y no llama Present ni crea swapchain. Orden SR: GL inputs -> NGX Vulkan -> shared output/semaphore -> GL shader pipeline composite7 y siguientes -> frame display -> presentación. El punto exacto de captura de FG se auditará sólo si standalone pasa; todavía no se declara combinación PASS.

Resultado efectivo 20261004-170642-750: DLSS seleccionado, pero debug/skip_init_vulkan=true heredado del TOML regenerado dejó RenderSystems.vulkan() null (RenderSystems.initVulkan retorna inmediatamente). GlVulkanInteropAlgorithm.createResources falló con NullPointerException antes de llamar NgxInitializer. El preparador verificó SR/FG/presentation pero omitió este prerequisito: defecto de preparación y de disponibilidad del selector, confianza alta. DLL propia empaquetada/extractada, JNI load CPU PASS; load/Init/capability/Create/Evaluate en cliente NO alcanzados. Evaluate/completion=0, output DLSS no demostrado. No se infiere falta de soporte GPU.

El usuario confirmó mundo/terreno/HUD visibles y cierre normal solicitado. El algoritmo fallido quedó sin instancia activa, según el catch de recreateAlgorithm; no existe evidencia de FSR disfrazado de DLSS. Launcher terminó con NTSTATUS0xC0000409, después de guardar todas las dimensiones. No hubo nuevo crash-report Java. Causa de ese fallo nativo al salir sin determinar; no se atribuye a NGX, que no se ejecutó. No PASS visual, no PASS de cierre. STOP aplicado: resize NOT_RUN, combinado NOT_RUN, sin retry posterior ni MFG nuevo.

Pendiente para otra iteración: exigir Vulkan inicializado para seleccionar DLSS, revisar disponibilidad real del selector (la verificación NGX en provider aún no se alcanzó), validar skip_init_vulkan=false como prerequisito de preparación y diagnosticar el cierre. No se corrigieron ni se volvieron a probar esos puntos después del FAIL.

Próxima fase MFG conservada: G2_IMAGE_GENERATION=PASS_BOUNDED_DIAGNOSTIC, G2_OPTIONAL_STATUS=UNKNOWN y x3=BLOCKED_STATUS_SEMANTICS_UNPROVEN. Pendiente semántica segura, luego x3 presentation, y sólo tras PASS investigar x4 count3/x5 count4.

Captura posterior aportada por el usuario: mundo reducido al rectángulo inferior izquierdo, grandes áreas negras arriba/derecha y HUD más ancho que la imagen del mundo. DLSS_SR_VISUAL_QUALITY=FAIL; DLSS_SR_BLACK_FRAMES=PARTIAL_BLACK_REGIONS_OBSERVED. La confirmación inicial de terreno/HUD visibles no implicaba salida correcta; se corrige explícitamente. El tamaño del rectángulo es compatible visualmente con render495x278 frente a display854x480, sin afirmar medición exacta ni output NGX.
