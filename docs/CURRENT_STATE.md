## Estado vigente — harness externo diseñado, 2026-10-03

Auditoría estática cerrada; ABI privada fuera del trabajo. Ninguna investigación adicional.
Un harness aislado: coordinador + workers nuevos; primero NGX Vulkan offscreen.
Stock original separado de capabilities adaptadas potencialmente intervenidas.
PASS sólo por Create/Evaluate count1-index1/completion/readback G1 válido != A/B.
Kernel creation requiere evidencia de logs; ausencia conserva UNKNOWN, no PASS inventado.
Variante D3D12 reservada; no diseño detallado ni sidecar hasta bloqueo SM86 Vulkan probado.
EXTERNAL_LOADER_READY=DESIGN_READY (runtime NOT_RUN); gates Vulkan=NOT_RUN.
IF_FAIL_STAGE=NONE_NOT_RUN; NEED_D3D12_SIDECAR=NO (no activado, necesidad aún indeterminada).
Diseño [DLSSG_EXTERNAL_HARNESS_DESIGN.md](DLSSG_EXTERNAL_HARNESS_DESIGN.md).
Sin implementación/ejecución/compilación, cambios globales driver/perfiles,
Minecraft, push/PR ni modificación de AMD_FSR_FG_X2=PASS.

## Estado anterior — integración externa DLSS-G SM86, 2026-10-03

Reverse engineering privado pausado. Host contract público localizado en fork ShyVortex.
External/AmpereMfgUnlock → INI generado → carga proxy sdli renombrado dlssg_sm86.dll.
Sin llamadas privadas GetInfo/Install/SM86Bridge desde el host público.
Carga no exige device D3D12 explícito; ejecución SM86 NGX Vulkan no demostrada.
Available/max pueden ser reescritos por el fork: no aceptar esos valores como PASS.
Sidecar D3D12: transporte documentado; FG offscreen sin swapchain pendiente de prueba.
InterpolationCount interno distinto de OverrideInterpolationCount en FG externo.
NVIDIA x2..x6 adaptado NOT_TESTED; ningún binario comunitario ejecutado.
Informe [DLSSG_EXTERNAL_HOST_AUDIT.md](DLSSG_EXTERNAL_HOST_AUDIT.md).
AMD_FSR_FG_X2=PASS intacto; sin cambios de runtime/mods/presentación.

## Estado anterior — auditoría gate NGX / backend SM86, 2026-10-03

Runtime detenido para investigación estática; ningún componente comunitario ejecutado.
Stock mantiene GPU0x170 < mínimo snippet0x190. No parche de capabilities ni spoof global.
Autor documenta game/Streamline spoof separado del mínimo exportado por el snippet
y sustitución de kernels SM86; 310.9 Original conserva kernels sm89 incompatibles.
Exports Install/GetInfo/SM86Bridge presentes en metadatos PE, contratos privados ausentes.
Valor Ampere del mínimo adaptado y mecanismo exacto no medidos; no inventar ABI.
SM86 Vulkan reproducible=UNKNOWN; pendiente instalación de kernels/host en esa ruta.
Auditoría [SM86_NGX_ARCHITECTURE_GATE_AUDIT.md](SM86_NGX_ARCHITECTURE_GATE_AUDIT.md).
AMD_FSR_FG_X2=PASS conservado; sin cambios de mods/PresentWorker/ring/interop.

## Estado medido — probe stock NGX DLSS-G Vulkan, 2026-10-03

PROBE_STOCK_CAPABILITY_QUERY=PASS en RTX3050Ti Laptop, driver596.49.
Runtime NVIDIA stock310.9.1 firmado y hash oficial verificado, ejecutable aislado.
NGX Init=Success; GetCapabilityParameters=Success. Available=0, NeedsUpdatedDriver=0.
FeatureInitResult=0xBAD0000B (UnableToInitializeFeature; getter Success).
MultiFrameCountMax=no reportado, getter0xBAD00010 UnsupportedParameter; no default inventado.
Callback NGX confirma GPU architecture0x170 < snippet mínimo0x190.
CreateFeature=SKIPPED por Available0; Evaluate=no implementado; NVIDIA generatedFrames=0.
SM86 probe=NOT_RUN: ABI/backend Vulkan reproducible no disponible. No hook instalado.
Resultado/evidencia logs/runtime/ngx-stock-probe/20261003-141624/result.json.
Informe [NGX_DLSSG_VULKAN_PROBE.md](NGX_DLSSG_VULKAN_PROBE.md).
AMD_FSR_FG_X2=PASS conservado; hashes intactos. Sin cambios en mods/PresentWorker/ring/
interop, reconstrucciones aprobadas, .minecraft real, push o PR. OptiScaler OFF.
No multiplicador NVIDIA PASS por capability ni x2..x6 ejecutados.

## Estado anterior — auditoría DLSS-G MFG, 2026-10-03

IMPLEMENTATION_STOPPED por instrucción del usuario; auditoría estática solamente.
Contrato oficial confirmado: count=N adicionales, index=1..N consecutivo,
una Evaluate por índice sobre el mismo par real. Mapeo x2..x6: count=1..5.
NGX Vulkan pasa ambos campos; output offscreen y pacing pertenecen al host.
Orden compatible conceptualmente con PresentWorker: A -> G1..GN -> B retenido.
No supone ejecución local ni certifica PresentWorker para MFG.
MultiFrameCountMax debe consultarse: techos comunitarios documentados 310.1=3,
310.9.1=5; traza secundaria 310.9.1 original_max=5/reportado=2 y otra cap=3.
RTX3050Ti/Vulkan max local=UNKNOWN, consultas nuevas=NOT RUN. X2..X6 NVIDIA=PENDING.
Falta conexión SM86/Vulkan reproducible, ABI privada publicada y permisos resueltos.
Informe: [DLSSG_MFG_AUDIT.md](DLSSG_MFG_AUDIT.md); evidencia logs/research/dlssg-mfg.
AMD_FSR_FG_X2=PASS; hashes JAR/bridge intactos. Sin cambios de mod/runtime,
rebuilds, pruebas Minecraft, nuevos proxies, .minecraft real, push o PR.
DLSS SR no es el objetivo de esta fase. No se habilitó x3..x6.

## Investigación anterior — DLSS-G Ampere, 2026-10-03

AMD_FSR_FG_X2=PASS conservado, hashes JAR/bridge intactos; ningún gate repetido.
Investigación DLSS-G RTX3050Ti: D — UNKNOWN / MORE RESEARCH NEEDED para SM86→Vulkan.
API NGX Vulkan directa existe, pero backend SM86 principal no publica fuentes y
el parche comunitario Vulkan auditado deja pendiente el adapter NVIDIA real.
No provider nuevo, proxy DLL, recompilación ni ejecución. OptiScaler OFF.
DLSS SR: hardware oficialmente soportado; proyecto BLOCKED por JNI NGX ausente
del JAR core-only (SR_NGX=OFF). Runtime SR NVIDIA 310.9 presente y firma Valid.
No confundir ese fallo anterior a feature creation con el soporte oficial de FG.
Redistribución de kernels/runtime adaptados sin autorización resuelta: pendiente.
Informe, diseño condicionado y siguiente paso: [DLSSG_AMPERE_RESEARCH.md](DLSSG_AMPERE_RESEARCH.md).
Primera etapa: 7 repos fijados y 197 textos verificados sin descargas binarias.
Después se auditó estáticamente un proxy fijado en deep/components, sin ejecutarlo,
instalarlo ni empaquetarlo; metadata/hash del runtime/backend embebidos guardados.
En la fase MFG posterior no se descargaron DLLs. Evidencia logs/research/dlssg-ampere.
No .minecraft real, push ni PR.
No quedó aplicado el interpolador temporal de la edición interrumpida.
Metadata .git actualmente ausente en ambos árboles; no se recreó ni alteró.

## Estado baseline conservada — AMD FSR FG x2 PASS, 2026-10-02

Auditoría temporal terminada: conclusión C, un interpolado por dispatch directo
en SDK 1.1.4; el swapchain Vulkan tampoco habilita multiframe. Los providers FG
3.1.5/3.1.6 auditados mantienen output0 y punto medio fijo. `outputs[4]` no prueba
soporte x3–x5. Recomendación: conservar 3.1.4 x2, sin implementación/migración.
Evidencia y clasificación: [FSR_3_1_4_MULTIFRAME_AUDIT.md](FSR_3_1_4_MULTIFRAME_AUDIT.md).
Repos y hashes de JAR/bridge verificados intactos; ningún gate runtime repetido.

Gates conservados, sin repetir: COMPLEMENTARY_OPENGL=PASS; VULKAN_INITIALIZATION=PASS;
GL_VK_INTEROP=PASS; PHASE_C=CLOSED; metadata inmutable y timing tests=PASS;
wisteria:fsr scaffold=READY; bridge JVM/context create/destroy=PASS.
Commits locales: SR f0e41bf8; Wisteria d29f6b8 y 299bfff. Sin push ni PR.

Prepare/dispatch JNI y adapter worker con output leases implementados, compilados y ejecutados.
AMD_FSR_3_1_4_CONTEXT=PASS; AMD_FSR_FG_GENERATED_FRAME=PASS; AMD_FSR_FG_X2=PASS.
4912 REAL + 4912 AMD GENERATED presentados; 4911 intervalos sin reset. Orden y
correspondencia VkImage/dispatch/submit/completion verificados para todos los pares.
70.00 FPS render reales, 140.23 FPS presentados en ventana de 70.05 s; GPU FG
0.660 ms (última media móvil 256 muestras de prepare+dispatch+copia real+barriers).
Observación breve del mundo completo y HUD alineado, sin negro/corrupción/flicker
anormal observado; cero errores nuevos registrados. Cierre normal, mundo guardado.
Evidencia logs/runtime/fsr-x2-{pass.log,world.jpg,result.json}; NO stress test.
JAR SR seleccionado: dev.f0e41bf8.opengl-fg-backpressure, SHA256
20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896.
JAR Wisteria fsr-backpressure: ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53.
Commits de implementación validados: SR e98b3fdb907a84f9b9fc0d5f6f45f7846158ba4c;
Wisteria 322ff7acc733c3699dd161a36f143584afcc157f. Árboles limpios.
Bridge x64 b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b.
DLL AMD oficial fijada por hash, empaquetada/cargada automáticamente; OptiScaler OFF,
sin selector DLL. FSR configura NO_SWAPCHAIN_CONTEXT_NOTIFY; SR sigue presentando.
Instancia exclusivamente runtime-baseline/instance: Complementary, SR1.7, Vulkan,
wisteria:fsr_fg/wisteria:fsr, X2, low latency NONE. Sin DH/SSRD/.minecraft real.
Trabajo detenido tras x2 según instrucción. No x3/x4/x5/x6. Revisar resultado antes
de cualquier fase nueva. Outputs 0..5 preparados; capacidad publicada/runtime=1.
Upscaling y FG permanecen en registros/grupos independientes; FSR1 de esta prueba
es la baseline legacy/fallback ya validada, no se convirtió en FSR2/3 por activar FG.
Remotes upstream SR y Wisteria retirados según autorización previa; sin push/PR.
Historial, licencias y procedencia conservados; refs anteriores en logs/runtime/*-before-upstream-detach-refs.txt.
No repositorio propio configurado: esperar URL del usuario y revisión distributiva.

Las siguientes secciones conservan antecedentes históricos; no representan el estado vigente.

## Vulkan inicializado — preflight de interop, 2026-10-02 (histórico)

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


# Estado recuperado - 2026-10-02

Última prueba: Complementary exacto activo y mundo cargado, **FAILED_VISUAL_GATE** por composición parcial/negro arriba-derecha y error GL1280 de formato. SR procesó superresolution.v1.json (prioridad sobre archivo sin versión); dispatch y bindings reales todavía sin validación individual. No se corrigió nada ni se probó resize/fullscreen/Vulkan. Baseline sin shader/core conservada; artefactos y commit JNI intactos. Ver COMPLEMENTARY_OPENGL_TEST.md. Próximo paso es diagnóstico de pipeline/viewport/formatos, no FSR FG/metadatos.

Estado posterior vigente: **CORE_RUNTIME_PASSED_OPENGL**. Core native construido/empaquetado/cargado; SR/Wisteria inicializados, menú/mundo alcanzados. JAR51c22ce1517002ade6bd891e1d104fed50348e8ffc28dc2f5cc8f557482593e0, core80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec. JNI configurable revisado/commit local03c0e447d37a8369f5e8864199bd971ec3a03f4b. Baseline gráfica ABIERTA: Complementary/Vulkan/interop pendientes. FSR FG/metadatos NO implementados. Ver recuperación posterior en RUNTIME_BASELINE.md.

Las secciones siguientes conservan las iteraciones anteriores, incluido «Gate runtime fallido». La actualización posterior al inicio y RUNTIME_BASELINE.md describen el estado vigente.

## Evidencia anterior a esta iteración

- Super Resolution branch: `dev`.
- Super Resolution commit: `173f809f92f5e88cc3f0a1c8d1b4ec695a3b4d19`.
- Super Resolution build: NO COMPILED. Dos logs anteriores, ambos fallidos; no existe JAR de baseline en neoforge/build.
- Wisteria branch: `main`.
- Wisteria commit: `5997653a3bddbc15c10ec9a619791fde25151a00`.
- Wisteria build: sin evidencia anterior de compilación; neoforge/build no existía.
- Archivos modificados / trabajo sin commit: únicamente superresolution/fabric/build.gradle.kts, plugin `fabric-loom` cambiado a `fabric-loom-remap`. Wisteria limpio; sin archivos no trackeados en ambos repositorios.
- El cambio Fabric lo genera settings.gradle.kts al seleccionar una versión obfuscada; se conserva, no se revierte ni atribuye a una implementación de FSR.
- Fase terminada: obtención de ambos repositorios y .gitignore; inicio de DEVELOPMENT_LOG.md.
- Fase incompleta: baseline (fase C); fases D-G no documentadas ni verificadas.
- Último error anterior: Fabric Loom 1.18.2 requiere JVM 25; Gradle se ejecutó con JVM 21. El primer intento buscaba configs/1.json por una selección incorrecta de versión.
- Próximo paso: terminar ambas baselines antes de diseñar o implementar FG.

## Correcciones a la documentación anterior

DEVELOPMENT_LOG.md decía «pending clone»: ambos repositorios existen con origin oficial y los SHA indicados. Decía Java 25 seleccionado: JAVA_HOME ahora señala JDK 21.0.12. También está instalado JDK 25.0.4. El target de Minecraft y el compilador del mod deben permanecer en Java 21; la JVM que ejecuta Gradle puede necesitar Java 25 por sus plugins. No se ha cambiado código gráfico.

## Política Git del usuario

No push, no PR, ni publicación a upstream. `origin` conserva temporalmente URL de fetch para trazabilidad; su push URL es `DISABLED_UPSTREAM_PUSH` en ambos repositorios. Al finalizar la obtención del material necesario, eliminar origin. Configurar el repositorio del usuario solo cuando lo facilite. Conservar historial y licencias. La raíz del workspace no es un repositorio Git.

## Validación y límites

No hay evidencia de INITIALIZED, RAN IN MINECRAFT, GENERATED FRAMES, VISUALLY VALIDATED ni STRESS TESTED. Los JAR de la instalación real son solo referencias y no prueban una baseline local. No se ha iniciado Minecraft ni añadido DH/SSRD a un entorno mínimo. Las configuraciones upstream incluyen DH entre dependencias de desarrollo; no ejecutar runClient con ellas sin preparar primero un perfil mínimo.

PDF de 17 páginas leído completo: coincide con las dos solicitudes pegadas. Se usa como documento de referencia; las instrucciones autorizadas proceden de la solicitud del usuario. No se localizaron BASELINE.md, FRAME_GENERATION_ARCHITECTURE.md, REFERENCE_DIFF.md, AMD_FSR_FG_PLAN.md ni MINECRAFT_RENDER_AUDIT.md anteriores en workspace/attachments.

## Continuación realizada

- Super Resolution HEAD actual: `195430dc2c7ed4fadea3b6d88734a5ccd25effdb` (`dev`). Commits locales: 02b36fe3 preserva cambio Fabric heredado; 195430dc añade selección opcional de loader.
- Wisteria HEAD actual: `aeed1ef78e23ef0f8de51ad65d8804af17457f61` (`main`). Commit local aeed1ef añade selección opcional de loader.
- Ambos working trees limpios tras commits. No push. Enlaces de fetch todavía presentes; destinos push bloqueados.
- `-Ploader=neoforge` evita configurar Fabric; permite ejecutar SR con Java 21 sin exigir Loom/JVM 25. Sin propiedad mantiene comportamiento de selección original.
- Super Resolution build actual: NO COMPILED. Último error attempt5: bloqueo de antivirus Windows en descarga LWJGL 3.3.4 natives-windows-x86; common/neoforge createMinecraftArtifacts fallan antes de compilar.
- Próximo paso: resolver la detección externa con revisión del proveedor/protección y completar baseline; no avanzar fases D-G ni copiar backend de referencia mientras falte esa evidencia.

## Cierre de la iteración

- Wisteria build actual: COMPILED. attempt3 BUILD SUCCESSFUL en 3m 6s; JAR identificado y bytecode Java 21 confirmado en BASELINE.md. Tests NO-SOURCE; ninguna validación gráfica.
- Fases A y B: recuperación e inventario completados. C: Wisteria compilado, SR bloqueado. D-G pendientes según secuencia solicitada.
- Último error pendiente: detección Windows al descargar LWJGL natives-windows-x86 3.3.4. No es un error del nuevo provider: aún no se implementó.
- Archivos modificados ahora: settings.gradle.kts de ambos repositorios (commits locales), configuración Git local de ambos (push bloqueado), docs/DEVELOPMENT_LOG.md (append-only). Nuevos: docs/CURRENT_STATE.md, docs/BASELINE.md, docs/REFERENCE_INVENTORY.md y logs de intentos 3-5 SR / 1-3 Wisteria. Historial previo y referencias conservados.
- Ambos repositorios quedaron limpios; documentos/logs están en la raíz no versionada. No se inicializa otro repositorio sobre ambos checkouts.

## Baseline compilada y arquitectura analizada

- Super Resolution branch: dev; commit: e365f5b32df88be8fb9925aaf0491658ebf4f705. Build: **COMPILED**, attempt6 BUILD SUCCESSFUL en 4m 21s, Java 21, NeoForge 1.21.1, perfil explícito Windows x64.
- Wisteria branch: main; commit: aeed1ef78e23ef0f8de51ad65d8804af17457f61. Build: **COMPILED**, baseline anterior reutilizada.
- Baseline conjunta COMPILED cerrada. Ambos JAR verificados y copiados a builds/baseline; detalles/hashes en BASELINE.md y artifact-verification.json. Sin inicialización ni validación runtime/visual/stress.
- Causa del bloqueo: Defender Trojan:Win32/Vigorf.A, cuarentena del temporal Java. Classifier x86 procedía de metadata multi-arquitectura Windows de minecraft-dependencies, forzado por SR a LWJGL 3.3.4. No es necesario en host/JVM x64. No se declara falso positivo.
- Acción: regla pública Gradle opcional WindowsX64MinecraftNativesRule, solo clientWindowsNatives de minecraft-dependencies y classifiers LWJGL x86/arm64. Versiones y empaquetado conservados; selección sin perfil intacta. Auditoría offline confirma clases y natives x64 3.3.4 en ambos módulos.
- Sin eliminación de caché, --refresh-dependencies, cambio de antivirus/exclusiones o sustitución de archivos. Consulta estado Defender/Get-MpThreatDetection denegada; eventos operativos sí accesibles y exportados.
- Archivos de implementación modificados: SR build.gradle.kts y nuevo buildSrc/src/main/kotlin/multiversion/WindowsX64MinecraftNativesRule.kt, commit e365f5b3. Wisteria sin cambios. Ambos árboles limpios; push upstream bloqueado, fetch temporal conservado.
- Documentación añadida: LWJGL_BLOCK_DIAGNOSIS.md, FRAME_GENERATION_ARCHITECTURE.md. BASELINE.md y DEVELOPMENT_LOG.md actualizados sin borrar historial. Nuevos logs diagnóstico/build/auditoría y copias baseline.
- Fase terminada: A/B recuperación/inventario, C compilación de ambos, D mapa estático de registro/provider/scheduler/presentación/interop y Wisteria.
- Correcciones verificadas: paquete actual api.registry.framegeneration; AsyncFrameGenerationScheduler antiguo no existe en checkout, sustituido por AsyncFramePresenter + FrameGenerationWorker + PresentWorker. Selección efectiva por prioridad, no solo inserción del registro. Provider no expone reset/resize separados: reset snapshot y claves de sesión/recursos.
- Fases incompletas: E comparación dirigida completa de referencias, F investigación/diseño oficial AMD, validación runtime mínima y auditoría nativa exhaustiva. G **no iniciada** por instrucción del usuario.
- Último error: bloqueo LWJGL anterior resuelto para este perfil; sin error pendiente de build. Avisos de deprecated/unchecked y fallback NFRT a librerías originales Minecraft documentados.
- Próximo paso: comparación dirigida de referencias contra la API actual y preparación de validación mínima sin DH/SSRD. No implementar FSR FG todavía; no modificar .minecraft real.

## Comparación y diseño AMD terminados — estado vigente posterior

- Comparación dirigida registrada en REFERENCE_DIFF.md; análisis javap y DLL estática en logs/design. No auditoría completa de matemática/crashes; informe MINECRAFT_RENDER_AUDIT y crash reports previos no localizados.
- SDK recomendado para investigación: FidelityFX1.1.4 / FSR3.1.4 Vulkan Windows x64, DLL AMD firmada. Header y fuente respaldan dispatch sin notificar proxy swapchain; APPLICATION_MANAGED_ASYNC sigue candidato sujeto a prueba de DLL/runtime/history/layouts.
- FSR_PROVIDER_CONTRACT.md define matriz real/MISSING, un generado, snapshots, sesión, recursos/leases, threads, barriers y fallback/reset/recreate. No provider FSR, JNI, C++, DLL ni shader implementados.
- THIRD_PARTY_ORIGINS.md documenta GPL3+ SR/Wisteria, LGPL native, MIT AMD y componentes/avisos con pendientes. Cinco gitlinks SR vacíos y header JNI Oracle vendorizado con términos incompletos impiden afirmar cierre distributivo.
- UPSTREAM_MIGRATION.md: NOT_READY_FOR_OWN_REPOSITORY; origin fetch upstream temporal, push sentinel disabled. Sin migración, push/PR/publicación. Raíz docs fuera de Git de ambos módulos.
- Baseline conjunta sigue COMPILED/Java21; hashes sin cambio. INITIALIZED/RAN IN MINECRAFT/GENERATED FRAMES/VISUALLY VALIDATED/STRESS TESTED no acreditados.
- Siguiente paso recomendado: preparar validación mínima y metadata immutable timing/color, completar provenance/package AMD y prueba del direct API antes de dispatch FG real. No modificar .minecraft, referencias o antivirus.

## Gate runtime fallido — estado vigente 2026-10-02

- Instancia totalmente separada en runtime-baseline/instance; launcher Gradle independiente utiliza ambos JAR baseline y solo Iris1.8.8, Sodium0.6.13, Architectury13.0.6, jcpp1.4.14 y dependencias del juego/anidadas. No DH/SSRD/shader custom/modpack. No cambios de source upstream.
- Java21.0.12 Windows x64, Minecraft1.21.1, NeoForge21.1.219. OpenGL RTX3050Ti Laptop/4.6 NVIDIA596.49 confirmado por log. Vulkan device no llegó a validarse; default config skip_init_vulkan=true y presentación OPENGL.
- Primer arranque solicitó reinicio automático earlyWindowControl=false solo en instance/config/fml.toml; segundo alcanzó Wisteria initializing e Iris, después falló SR preInit: recurso mandatory lib/libSuperResolution+win64+release.dll ausente del JAR. Auditoría confirma que baseline SR no contiene DLL propia; VMA natives siguen dentro JAR anidado, son distintos del core SR.
- INITIALIZED conjunto: NO; RAN IN MINECRAFT al menú/mundo: NO ACREDITADO; GENERATED FRAMES/VISUAL/STRESS: NO PROBADOS. Registro FG/providers, Vulkan/interop, resize/world/presenter/capture ring: NO ACREDITADOS.
- Error es empaquetado/source native faltante, no bloqueo antivirus demostrado. No se quitaron checks, no se cargó WisteriaNative antigua, no se modificó la instalación real. Procesos de prueba detenidos al aviso; sin hs_err nativo observado hasta entonces.
- Fases2/3/4 posteriores al gate: NOT_STARTED_RUNTIME_GATE_FAILED. Sin paquete AMD descargado, Authenticode evaluado, JNI/C++/DLL nueva, dispatch, metadatos o tests/commits de ese cambio.
- Próximo trabajo únicamente cerrar native core y empaquetado baseline con procedencia/license/source verificadas, verificar recurso required y volver a probar runtime. No sustituirlo con referencia antigua. Investigación AMD y contrato siguen propuestas estáticas.
## Continuación FG — 2026-10-02

COMPLEMENTARY_OPENGL=PASS; VULKAN_INITIALIZATION=PASS; GL_VK_INTEROP=PASS;
PHASE_C=CLOSED. Se cargó New Worldsdfsdf con Complementary, SR1.7, presentación
Vulkan genérica y FG OFF: observación breve sin negro/corrupción/flicker anormal,
HUD alineado, sin crash/nuevos errores de sincronización registrados. No stress test.
Evidencia: logs/runtime/phase-c-progress.json y phase-c-generic-vulkan-world.{jpg,log}.
JAR2003c3d0588650a009dde3ae31f5bab8e282ad674e78a66057d3de2711ae1c3a.
Commits locales ea0e5169 (Streamline requerido sólo si se inicializa) y 2ae956e6
(logs acotados de interop). La cadena async QUEUED/DISPATCHING todavía debe probarse
con el provider; FG OFF usó SEALED→SUBMITTED→REUSABLE.
SDK AMD oficial1.1.4 descargado: archive0216556bfb0e243cec30004a2a98d38f4e3f7406cb7938e3c1b85c758e95d952,
DLL Vulkan c84ca40421ff5594edfc6553cb89a2d07982b0cadb1a66beb989b2cdedd39197.
Origen/tag/headers/notices en third_party/amd-fidelityfx-1.1.4/manifest.json.
Commit SDK c6efa6bf7f2027b3ec94f28578bb5965eabb9e55. Authenticode=NotSigned;
no afirmar firma AMD. Bridge/context/dispatch AMD aún pendientes; generated=0.
Siguiente: auditar inputs y capturar metadata inmutable; implementar wisteria:fsr
y nuevo bridge JNI usando exclusivamente la API Vulkan1.1.4, primer x2 y detener.
Sin push/PR/DH/SSRD ni modificación de .minecraft real. Las secciones debajo son
históricas y el anterior bloqueo de Streamline ya se resolvió.
