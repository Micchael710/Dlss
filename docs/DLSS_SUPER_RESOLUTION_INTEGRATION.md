# Integración NVIDIA DLSS Super Resolution

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
