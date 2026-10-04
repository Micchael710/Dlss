# Development Log

This file is append-only. Build status labels distinguish `COMPILED`, `INITIALIZED`, `RAN IN MINECRAFT`, `GENERATED FRAMES`, `VISUALLY VALIDATED`, and `STRESS TESTED`.

## 2026-10-02 - Workspace protection and baseline start

- Super Resolution commit: pending clone.
- Wisteria commit: pending clone.
- Changes: inspected the initially empty workspace; added repository hygiene rules; recorded the supplied reference artifacts without modifying them.
- Build result: pending.
- Errors: Java 25 is currently selected; the requested build target requires Java 21. Installed toolchains are being inventoried before building.
- Decisions: treat every artifact under the supplied `.minecraft` paths as read-only; do not copy or alter the shader until analysis requires a workspace copy; use upstream build configuration for Minecraft 1.21.1 before evaluating NeoForge 21.1.251.

## 2026-10-02 - Recovery of interrupted baseline

- Recovered Super Resolution dev at 173f809f92f5e88cc3f0a1c8d1b4ec695a3b4d19 and Wisteria main at 5997653a3bddbc15c10ec9a619791fde25151a00. Earlier pending-clone entries are historical, not current status.
- Preserved existing Fabric plugin-id change; settings generates it for obfuscated Minecraft. No existing changes discarded.
- Read both pasted requests and all 17 PDF pages; inventoried reference SHA-256 and shader integration without modifying Minecraft.
- Previous failure: malformed version selection first, then Loom requiring JVM 25 when Gradle ran on JVM 21. Continued baseline using explicit version arguments, existing Gradle cache and Java 25 host for SR / Java 21 host for Wisteria. Java 21 remains mod target.
- User clarified ownership: no upstream push or PR. Both origin push URLs changed to DISABLED_UPSTREAM_PUSH; fetch retained temporarily for provenance and will be removed after source acquisition is complete. No new remote supplied yet.
- Added CURRENT_STATE.md, BASELINE.md and REFERENCE_INVENTORY.md. Architecture/AMD research/scaffold remain gated on both compiled baselines.

- SR attempt4 passed the JVM version gate but failed remapping two unrelated Fabric mods. Preserved inherited generated Fabric change in local commit 02b36fe3, then added optional supported-loader selection (SR 195430dc / Wisteria aeed1ef).
- SR attempt5 with Java 21 and NeoForge-only selection failed before source compilation: Windows antivirus blocked LWJGL 3.3.4 windows-x86 download. No protection settings or graphics behavior modified.
- Wisteria attempt2 was interrupted before compilation; attempt3 resumed with NeoForge-only selection and Java 21. See BASELINE.md for final result.

- Wisteria attempt3 BUILD SUCCESSFUL in 3m 6s; artifact SHA-256 E41E1CF694C2E0A2724E7C887BED44D5C35818616A4B3A54C6561DAFC4AF0D29, class major version 65. COMPILED only; tests NO-SOURCE and runtime/visual/stress statuses unverified.
- Both Git trees clean, all changes local. Reference hashes rechecked unchanged. Combined baseline remains incomplete due to SR antivirus block; AMD architecture investigation and scaffold deferred according to the user's phase gates.

## 2026-10-02 - Safe LWJGL diagnosis and compiled baseline closure

- Traced common/neoforge createMinecraftArtifacts artifactManifestEntries to neoFormRuntimeDependenciesRuntimeClasspath, NeoForm, minecraft-dependencies/clientWindowsNatives and SR's forced LWJGL 3.3.4. Offline dependencyInsight and exact cached metadata confirmed it requests all Windows architectures.
- Defender events 1116/1117 identified Trojan:Win32/Vigorf.A and successful quarantine of gradle_download15245544113965602401bin, downloaded by Java 21. Exported matched events only. Threat/status cmdlets denied access. No antivirus settings changed and no false-positive claim.
- Confirmed x86 native is not required by Java compilation/x64 execution or SR jarJar packaging. Existing natives-windows DLL verified as PE AMD64. No partial x86 cache artifact remained; no deletion/refresh needed.
- Explained before editing, then committed e365f5b3: optional sr_native_arch=windows-x64 metadata rule using public Gradle artifactSelectors; removes only LWJGL x86/arm64 from clientWindowsNatives. Validated host/target guards; no version/graphics changes. Without property default resolution stays intact.
- SR attempt6 BUILD SUCCESSFUL in 4m 21s. Offline artifact audit passed common/neoforge; base/natives-windows remain 3.3.4. NFRT's internal fallback to Minecraft-original 3.3.3 library manifest is separately documented and was not changed.
- Verified SR ZIP/metadata/1420 Java 21 classes and five embedded JARs; copied both baseline JARs to builds/baseline with hashes. Wisteria baseline reused, no repeat build. SR hash 14BEDCFE2DE3936E3485E441453896FC1F55B55CC222043947A9AA72D8E0B882. No runtime/visual/stress validation claimed.
- Added LWJGL_BLOCK_DIAGNOSIS.md and FRAME_GENERATION_ARCHITECTURE.md; corrected old package/scheduler names and effective priority selection against actual code. Mapped capture ownership, worker submissions/output leases, pacing, swapchain recreation, NGX/Streamline and Reflex. Static mapping only, native safety still requires runtime evidence.
- No AMD FSR FG implementation, no push/PR, no change to real Minecraft/reference files. Both Git trees clean, upstream push URLs remain disabled. Next: reference comparison and minimal runtime validation preparation.

## 2026-10-02 - Directed reference comparison and AMD provider design

- Rechecked baseline hashes unchanged and both source Git HEADs/trees/remotes unchanged; origin push remains DISABLED_UPSTREAM_PUSH. No rebuild needed for documentation-only work; all runtime statuses still unverified.
- Read the new pasted user request. Targeted javap inspected legacy SR registry/provider/request/scheduler and Wisteria FSR registration/adapter/bridge/configuration. Corrected old scheduler package to common.presentation.vulkan when locating reference class. Initial restricted Java read failed access checks; repeated only targeted read with approved escalation. No JAR modified.
- Saved directed analysis in logs/design. Static DLL inventory corroborates custom Vulkan compute pipeline (30,720 bytes, four bridge exports); did not execute native code or claim a full reverse engineering of shader math/crash cause. MINECRAFT_RENDER_AUDIT and prior GPU crash reports were not located in available material.
- Created REFERENCE_DIFF.md with per-difference classification and current equivalents. Current FrameGenerationGroups has only DLSS_FG; proposed wisteria:fsr and wisteria:fsr_fg do not collide statically, but old reference must stay outside the new runtime.
- Consulted official AMD releases/manuals/headers/license: selected SDK1.1.4 / FSR3.1.4 / Windows x64 Vulkan for investigation, rather than current2.3 DX12 ML effects. FFX header has NO_SWAPCHAIN_CONTEXT_NOTIFY and direct outputs, making application-managed async a concrete candidate; null swapchain/configure/history and signed DLL behavior remain gates. The documented proxy path has distinct queue/pacing ownership and cannot be silently inserted into current async provider.
- Created AMD_FSR_FG_PLAN.md and FSR_PROVIDER_CONTRACT.md: immutable timing/color gaps, actual resources, ownership/threads/sync/leases, session/reset/resize/failure/fallback and proposed native boundary. No JNI/C++/DLL/shader/dispatch implementation.
- Created THIRD_PARTY_ORIGINS.md with provenance, local licenses/notices and explicit unresolved redistribution/SDK/submodule items. Five SR gitlinks have empty local directories. Wisteria declares GPL3+ but no root LICENSE or notices were found. Baseline Wisteria ships no native DLL, so build does not prove runtime support.
- Created UPSTREAM_MIGRATION.md: NOT_READY_FOR_OWN_REPOSITORY because sources/submodules/package/license closure and full native reproducibility are incomplete. No remote migration or publication executed. Root docs remain outside the two Git repos; no root Git repo invented.
- Next authorized iteration should address metadata capture/proof of AMD direct API and baseline runtime preparation separately; dispatch implementation requires the user's next instruction. DH/SSRD excluded from planned minimal instance, real installation untouched.
- Follow-up source review confirmed official Configure skips swapchain configuration with NO_SWAPCHAIN_CONTEXT_NOTIFY and dispatch records OF/interpolation into the supplied commandList using outputs[0]. Runtime/history buffering remains unproven. Native header audit corrected JNI attribution: vendored jni_win64 declares Oracle proprietary/confidential rather than an assumed OpenJDK license; documented provenance/redistribution gap. GLAD SPDX and Vulkan header Apache2 notices recorded from actual headers.

## 2026-10-02 - Isolated runtime baseline gate failed

- Read the new pasted request and respected phase ordering: runtime must pass before acquiring AMD package or implementing metadata. Created standalone Gradle launcher in runtime-baseline using baseline JARs, MC1.21.1/NeoForge21.1.219/Java21 and only configured Iris/Sodium/Architectury/jcpp plus game/transitive libraries. Original repositories unchanged; optional native-selection rule matches existing Windows x64 policy.
- Launcher preparation initially lacked Architectury repository; added upstream official maven.architectury.dev. auditRuntimeProfile passed after resolving official dependencies. No DH/SSRD/modpack/old custom JAR/shader added. Minecraft assets cached in Gradle, game files exclusively workspace instance; no real .minecraft changes.
- First launch requested restart because SR automatically changed isolated earlyWindowControl to false. Preserved first log, stopped test process in its info dialog, restarted once. This is the explicit NeoOpenGLVersionOverride bootstrap path, not an antivirus issue.
- Second launch discovered exact baseline versions and Wisteria entrypoint, then NativeLibManager failed at 15:55:22: required embedded core DLL lib/libSuperResolution+win64+release.dll not found. Stopped the test process at error dialog. No menu/world or Vulkan/interop validation achieved. No hs_err/crash report observed before stop; no stability claim.
- Confirmed shader-free GL renderer NVIDIA RTX3050Ti Laptop / OpenGL4.6 / driver596.49. Host GPU report restricted to model/driver. SR defaults presentation OPENGL and skip_init_vulkan=true; even a successful default menu run alone would not demonstrate Vulkan interop.
- Read-only ZIP audit found zero direct SR native DLL entries and unchanged hashes. Native build/copy tasks are explicit; neoforge:build did not create native outputs and mustRunAfter alone does not select tasks. Did not run native build script, create a DLL, copy old native, remove required/capability checks, or modify antivirus.
- Saved runtime logs/error export, artifact/native presence audit and gpu-host. Export sanitizes user paths/tokens. Created RUNTIME_BASELINE.md and updated BASELINE/CURRENT_STATE/AMD plan/provider contract/third-party provenance with failure and evidence levels. Isolated instance excluded in root .gitignore.
- No phase2 SDK package/signature audit, no new phase3 direct proof/runtime test, no phase4 metadata/test/build/commit because runtime gate failed. Previous static AMD evidence remains static; signed DLL recommendation explicitly distinguished from unperformed Authenticode verification. Both repo HEADs/remotes unchanged, no push/PR/migration.
- Next: repair current baseline's required core native packaging with verified upstream source/build/provenance; re-run runtime gate before proceeding to SDK or FG metadata.

## 2026-10-02 - Native core recovery and interrupted graphical continuation

- Built SR_MAIN_LIB Release x64 only, upstream optional modules all OFF. Initialized only pinned FreeType5336c0d4da22a13dab3389eb153b12672fdf841c and glslang a8d28bd082bff18ffbe80996e922b012f915cf07 from configured upstream URLs. Acquired only init.py's glslang Windows Release package; hash and provenance preserved. No destructive platform script, old DLL or AMD FG bridge.
- Used installed MSVC19.51.36244/toolset14.51.36231, Windows SDK10.0.26100.0, CMake4.2.3-msvc3, Ninja1.13.2, JDK21.0.12.1 JNI headers; no global environment changes/toolchain installation. Compiler dependency audit excludes vendored Oracle platform headers. Bit-for-bit reproducibility and independent glslang binary reproduction not claimed.
- Core DLL6138880 bytes/AMD64/hash80ce6b622985e737731da8662cbc694e88f18a24605978caa394259dd0faccec; 161 exports including all133 JNI; only KERNEL32.dll imports; no debug CRT/PDB/absolute paths detected. copyNativeLib and neoforge build succeeded. New separate baseline hash51c22ce1517002ade6bd891e1d104fed50348e8ffc28dc2f5cc8f557482593e0; original preserved. Own Java classfiles identical/Java21.
- Runtime loaded core, initialized SR/Wisteria and registered ngx/streamline/reflex. User entered a test world; integrated server logged join16:30:57. Client subsequently saved/closed cleanly. OpenGL initialization passed, Vulkan skip remained true, FG OFF. User stopped earlier Computer Use via Escape; no visual/world completeness claim.
- New continuation reviewed exactly two JNI edits/status/diff--check and committed locally03c0e447d37a8369f5e8864199bd971ec3a03f4b with requested message. No rebuild solely for commit, no push/PR/fork/migration. Updated five requested documents preserving failed gate history.
- Relaunched same isolated runtime and copied exact Complementary ZIP, original/copy hash680e76ee11b0513c84602c3a3e151c64c236c5bc952fff4492265b8d4a4cf7d7. Static config bindings confirmed; actual activation pending. Capture recovery returned another window's pixels even after activation; no inputs on those pixels. Asked user to expose Minecraft; user requests secondary monitor. Vulkan test remains gated behind successful Complementary/OpenGL; no FidelityFX changes.

## 2026-10-02 - Complementary OpenGL activated; visual gate failed

- Resumed after explicit user authorization. Reset persistent Computer Use session to recover from earlier Escape cancellation; cancellation was not mod failure. Client had closed, so launched same isolated profile only; no rebuild/package changes. Preserved previous interrupted log separately. User moved new window to secondary monitor; observed actual Minecraft before clicks.
- Selected exact pack in Iris UI and applied. Runtime log16:47:14 confirms SR loaded superresolution.v1.json, which takes precedence over unversioned config;16:47:15 Iris Using shaderpack/HIGH confirms activation. Versioned config checked: autotex3/depthtex/autotex2, BEFORE composite7, rgba16f and output display-size region. Individual resolved texture handles/dispatch execution remain unproven.
- Entered existing world16:49:01, no new world. After compilation/chunk stabilization, scene occupied lower-left render-sized rectangle while rest was black/HUD display-sized. Reobserved persistent failure and saved visual evidence. GL1280 Internal format not supported at16:49:28 and16:53:29. No crash observed. Stopped motion/resize/fullscreen tests and did not proceed to Vulkan.
- Iris logged unresolved newer biome/endFlash uniforms and stale stone_slab variant. Source candidate: SR_ACTIVE forces FXAA_DEFINE=-1 while shader properties can disable trigger composite7 for that value; actual properties-preprocessor/pass behavior still UNKNOWN. Render/display/viewport/copy and exact GL error source need diagnosis. No repair or invented causal attribution.
- Exported complementary-opengl-errors.log plus sanitized game/client snapshots, selected config and structured result. Updated RUNTIME_BASELINE/CURRENT_STATE/BASELINE and this append-only log; added COMPLEMENTARY_OPENGL_TEST.md. Hashes shader original/copy, SR JAR and core unchanged. Source repos untouched, no new commit/push/PR/fork/migration. Client remains open for inspection. COMPLEMENTARY LOADED YES, VISUALLY_VALIDATED NO, GENERATED FRAMES NO, STRESS TESTED NO; graphical baseline OPEN and not ready for Vulkan gate.


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

## 2026-10-02 — Primera ejecución AMD FSR FG x2 PASS

- Gates aprobados conservados; no se repitieron OpenGL/interop/context probe/timing tests.
- SDK oficial1.1.4 empaquetado con notices y DLL fijada por SHA256; JNI propio x64,
  prepare CameraInfo + framegeneration dispatch, NO_SWAPCHAIN_CONTEXT_NOTIFY.
  OptiScaler OFF, sin selección DLL. PresentWorker conserva adquisición/presentación.
- Primer build corrigió narrowing C++ de rectángulos; primer run corregido de formato:
  pool RGBA8, conversión a BGRA8 del swapchain por PresentWorker. Archivo fallback
  conservado: logs/runtime/fsr-x2-output-format-fallback.log.
- Primer dispatch real produjo/presentó8 AMD frames pero expuso saturación de capture
  ring DISPATCHING. Evidencia fsr-first-generated-capture-ring-crash.log. Se corrigió
  con copia real propia, liberación de inputs por fence FG, espera de submission
  antes de reuse y metadata inmutable por lote. Dos pruebas de concurrencia PASS.
- Build Java21 SR y Wisteria PASS; sólo componentes cambiados, core/bridge de contexto
  no reconstruidos innecesariamente. Artefactos/hash en BASELINE.md y *-artifact.json.
- Run final: 4912 frames reales +4912 AMD generados, 4911 dispatch sin reset, todos los
  VkImage/submit/completion/pares GENERADO→REAL auditados. Mundo completo/HUD correcto,
  sin negro/corrupción/flicker anormal observado; cero errores nuevos registrados,
  cierre normal y guardado del mundo. No STRESS TESTED.
- Ventana70.05s: real render70.00 FPS, presentado140.23 FPS; FG GPU0.660ms última media
  móvil256 (prepare+dispatch+copia real+barriers). Evidencia fsr-x2-pass.log/world.jpg/result.json.
- Commits locales SR e98b3fdb907a84f9b9fc0d5f6f45f7846158ba4c y Wisteria
  322ff7acc733c3699dd161a36f143584afcc157f. Metadata ya separada en SR f0e41bf8.
- Remotes upstream retirados conforme a autorización previa. Historial/licencias/URLs
  y refs conservados; ningún push/PR/repositorio propio inventado. Árboles limpios.
- Trabajo detenido tras AMD_FSR_FG_X2=PASS. No x3/x4/x5/x6, DH, SSRD ni .minecraft real.


## 2026-10-02 — Auditoría temporal FSR 3.1.4 / 3.1.5 / 3.1.6 cerrada

- Investigación exclusiva de textos oficiales fijados a SDK 1.1.4, v2.0.0
  (f4c1da8e92f3fe563b5c28c44e6267ce6b6b8eb2) y v2.1.0
  (0836aa7f24058b3f8d035a88c0774457d78fc98e). Manifiestos SHA256 y diffs
  conservados en logs/research; hashes descargados verificados.
- Conclusión C: 3.1.4 ONLY SUPPORTS ONE INTERPOLATED FRAME PER DIRECT DISPATCH.
  outputs[4]/numGeneratedFrames no se consumen como bucle ni fases por el provider;
  output0, un dispatch FI y midpoint fijo. Swapchain Vulkan solicita uno.
- 3.1.5 y 3.1.6 mantienen output0 y las mismas fórmulas temporales. No hay
  comportamiento multiframe de esos providers que portar a Vulkan.
- Informe completo y clasificación de cambios: docs/FSR_3_1_4_MULTIFRAME_AUDIT.md.
  Recomendación actual: mantener x2; demostrar otra ruta oficial con fases múltiples
  antes de diseñar una implementación x3-x5. Ninguna ruta implementada.
- AMD_FSR_FG_X2=PASS conservado. SR/Wisteria limpios en commits validados;
  JARs y bridge con hashes intactos, sin remotes. Sin recompilaciones, ejecución
  Minecraft, modificación de runtime, habilitación x3/x4/x5, push o PR.

## 2026-10-03 — Investigación DLSS-G Ampere / Vulkan

- Investigación estática cerrada con resultado D para wisteria:dlssg_ampere.
  La adaptación principal describe runtime NVIDIA y kernels SM86, no FSR; su
  árbol público carece de fuentes CPU/kernels/build reproducible. No se afirmó
  portabilidad ni imposibilidad sin evidencia.
- Confirmada API NGX DLSS-G Vulkan directa con output recurso de la aplicación.
  Streamline Vulkan sí existe, pero su plugin controla acquire/present/pacing.
  El parche comunitario Pipo publica glue/ABI/loader; falta FeatureAdapter NVIDIA
  real y adaptación de kernels Vulkan. Nukem ejecuta AMD FSR; OptiScaler VkOn12
  aporta referencias de memoria/fences, no un gate DLSS-G SM86.
- DLSS SR RTX30 oficialmente soportado. Bloqueo local identificado antes de
  feature creation: JNI libSuperResolutionNGX ausente en JAR y build SR_NGX=OFF.
  Runtime nvngx_dlss.dll 310.9 presente, firma NVIDIA Valid. SDK headers locales
  ausentes. Init/capabilities/create/evaluate posteriores permanecen PENDING.
- Licencias/activos diferenciados: GPLv3 propio/Nukem/OptiScaler, MIT glue,
  runtime y kernels NVIDIA sin redistribución adaptada autorizada identificada.
  No DLL/model/kernels descargados, instalados, ejecutados ni empaquetados.
- 7 repos fijados por commit, 197 textos/hash verificados; manifests y evidencia
  local en logs/research/dlssg-ampere. Informe docs/DLSSG_AMPERE_RESEARCH.md incluye
  inputs, bloqueos, diseño condicionado y validación futura.
- AMD_FSR_FG_X2=PASS conservado; hashes SR/Wisteria/bridge coinciden. Sin cambios
  en PresentWorker, ring, interop, fuentes de mod, build o runtime. Sin repetir
  compilaciones/gates, sin .minecraft real, push/PR. No quedó patch temporal parcial.
- .git actualmente ausente en ambos árboles; no se recreó ni se afirmó git limpio.

## 2026-10-03 — Profundización SM86 y auditoría MFG; implementación detenida

- Antes del cambio de alcance se descargó el proxy main 0.3.5 fijado solamente
  para metadatos PE/hash, nunca LoadLibrary ni invocación de exports. Identificados
  runtime 310.9.1 SHA256 ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82
  y backend SHA256 1aebf8f306fbaede18c98b372da9c70a1bbdb1bd35327f7f85c2969085a619a1.
  Exports privados SM86Bridge_Install/DlssgMod_Install carecen de firmas/estructuras
  públicas suficientes. No se extrajeron ni ejecutaron kernels/runtime embebidos.
- El usuario detuvo cambios de runtime y pidió auditoría exhaustiva MFG. No se
  inició backend SM86 ni se construyó artefacto de ejecución. Fuente pública y
  PDF oficial solamente en esta fase; cero DLLs nuevas descargadas/ejecutadas.
- Guía oficial NGX 310.7.0, PDF físico 109, confirma una Evaluate por índice 1..N
  consecutivo, mismos inputs del par, count fijo durante el grupo y reset index1.
  PDF 25 confirma generados antes del real retenido. Vulkan helper transmite count
  e index en líneas 82/83 y evalúa una vez en 192. Exactas ocurrencias/hashes
  conservados en logs/research/dlssg-mfg/header-evidence.json.
- Mapeo semántico x2..x6 confirmado count1..5; ejecución local NVIDIA no probada.
  Máximos 310.1=3/310.9.1=5 son techos documentados comunitarios. Issue518 aporta
  traza original_max5/reportado2; discusión546 cap3 y create exitoso sin Evaluate
  seguido de crash. No elevar estos reportes a consultas propias ni universalizar5.
- Consulta local RTX3050Ti/Vulkan y punto interno de actualización de historia
  UNKNOWN. No se habilitaron opciones ni se alteró el pool/scheduler existente.
  No se ejecutó NGX/SDK/componente SM86 ni se abrió Minecraft.
- docs/DLSSG_MFG_AUDIT.md recoge rangos/defaults, secuencia, recursos, resets,
  capabilities, dependencia de plugin/ring, límites de evidencia y diseño conceptual.
  Sustituida recomendación centrada en DLSS SR por investigación FG/ABI, sin implementar.
- Verificación estática: 197 fuentes previas y hashes SR/Wisteria/bridge AMD
  coinciden. AMD_FSR_FG_X2=PASS conservado. Sin modificaciones de runtime/mod,
  recompilación, repetición de gates aprobados, .minecraft real, push ni PR.

## 2026-10-03 — Probe stock NGX DLSS-G Vulkan en RTX3050Ti

- Construido y ejecutado un probe C++ Windows x64 aislado, sin JNI/mod/Minecraft.
  Runtime310.9.1 y nvsdk_ngx_s.lib oficiales fijados por commit/Git blob SHA;
  DLL hashff6e90eb... y firma NVIDIA Valid verificados antes de cargar. Assets
  sólo bajo experiments/ngx-dlssg-probe, ignorados para versionado; sin proxies.
- Init y GetCapabilityParameters PASS. RTX3050Ti Laptop driver596.49, UUID/LUID
  conservados en raw-result. Getters Available=0, FeatureInitResult=0xBAD0000B,
  NeedsUpdatedDriver=0 todos con result Success. Max getter0xBAD00010 y valor null.
  No hardcode5 ni conversión de getter fallido a un max numérico.
- NGX callback muestra gate architecture0x170 versus mínimo0x190 para stock310.9.1
  y fallback del driver. 0xBAD0000B es UnableToInitializeFeature en SDK actual,
  no OutOfDate. FG unavailable es resultado esperado, probe exit0.
- CreateFeature omitido por Available0; Evaluate y generación no implementados.
  SM86 escenarioB NOT_RUN por ABI/backend Vulkan reproducible no disponible.
  Comparación stock/adaptado separada de futuras observaciones antes/después del hook.
- Evidencia logs/runtime/ngx-stock-probe/20261003-141624/ y docs/NGX_DLSSG_VULKAN_PROBE.md.
  Hashes/valores verificadas con summarize_ngx_stock_probe.py. Build aislado completó
  fuera del sandbox tras bloqueo de la comprobación del compilador dentro de él.
  No tests/gates Minecraft ni recompilación de componentes baseline.
- AMD x2 JARs/bridge hashes coinciden; mod/PresentWorker/ring/interop intactos,
  OptiScaler OFF. No .minecraft real, push, PR ni multiplicador NVIDIA habilitado.

## 2026-10-03 — Separación del gate NGX y adaptación SM86

- Investigación estática desde el log stock conservado, sin repetir el probe.
  Gate 0x170/0x190 verificado en callback L309–311 y L329–331.
- Separados spoof game/Streamline, mínimo del snippet, instalación del backend
  y arquitectura de kernels. La documentación del autor anuncia mínimo físico;
  ejemplo explícito Turing0x160. Ampere0x170 es consecuencia esperada de esa
  regla, no medición del backend ni evidencia de un campo ampere_host publicado.
- Metadatos PE previos: runtime embebido hash idéntico al stock oficial;
  GetInfo/Install/SM86Bridge_Install presentes en backend. No invocados.
  No fuentes/signaturas/layouts ABI reproducibles ni prueba de kernels Vulkan.
- 310.9 KernelImage=Original conserva kernels de imagen sm89; Optimized=0 aún
  sustituye imágenes compatibles. Pasar el mínimo aislado no valida kernels.
- Documentado UNKNOWN Vulkan y contrato exacto pendiente en
  docs/SM86_NGX_ARCHITECTURE_GATE_AUDIT.md. Main e init no exponen implementación;
  fork Vulkan delega FeatureAdapter, no demuestra adaptación SM86.
- Hashes baseline y 197 textos verificados; sin recompilación, runtime changes,
  ejecución comunitaria, selección DLL, modificación AMD/PresentWorker/interop,
  .minecraft real, push ni PR.

## 2026-10-03 — Host externo ShyVortex y diseño de sidecar D3D12

- Pausada investigación interna de ABI privada. Descargados sólo 1.430 textos
  públicos de tres repositorios, fijados por commit y Git blob/SHA256.
  Ningún build script descargado se ejecutó; ningún DLL comunitario descargado/cargado.
- Traza fork: Config External/Ampere flag → worker GPU → candidatos → INI junto
  al DLL → LoadLibrary. Paquete renombra version.dll sdli a dlssg_sm86.dll.
  No llamadas privadas de instalación desde host; otra variante admite ASI init
  y controles públicos opcionales. Worker no garantiza orden antes de NGX/slInit.
- External evita segundo FG interno; hooks de opciones aún pueden intervenir.
  Available y max del fork pueden ser anuncios forzados, incluyendo Vulkan.
  InterpolationCount interno no implica count externo; override/game y luego
  Streamline/NGX deben solicitarlo. No loop de indices comunitario demostrado.
- Carga sin device D3D12 explícito; kernels SM86 Vulkan UNKNOWN. Sidecar tiene
  transporte oficial y referencia de código shared resource/fence, pero requiere
  demostrar FG offscreen D3D12 sin swapchain antes de implementar.
- Informe docs/DLSSG_EXTERNAL_HOST_AUDIT.md; x2..x6 NVIDIA adaptado NOT_TESTED.
  AMD x2, PresentWorker/ring/interop intactos. No Minecraft, push ni PR.

## 2026-10-03 — Diseño del harness externo único, Vulkan primero

- Auditoría estática CLOSED; sin búsqueda nueva ni deducción de ABI privada.
- Diseñado coordinador/workers aislados, stock original separado de adaptado
  reported. Referencia stock existente reutilizable sólo con identidad compatible;
  no repetir el gate aprobado ni contaminarlo con hooks comunitarios.
- Contrato público carga+INI+init documentado cuando corresponde; módulo retenido
  durante proceso. Sin OptiScaler completo, perfiles/DRS ni callbacks privados.
- Variante A: NGX Vulkan offscreen, fixture temporal coherente, warmup corto,
  Create/Evaluate x2, submit/wait GPU y readback canónico contra A/B/sentinel.
  Separados kernel logs, creación de feature y generación confirmada; caps no PASS.
- Clasificación de fallo por evidencia, errores de harness distintos de SM86.
  Variante B D3D12 reservada hasta bloqueo causal demostrado en A; sin sidecar ahora.
- docs/DLSSG_EXTERNAL_HARNESS_DESIGN.md; todos los gates adaptados NOT_RUN.
  Sin implementación, binarios ejecutados, compilación, modificación AMD x2,
  driver/perfiles, Minecraft, push o PR.


## 2026-10-03 — Recuperación GPT, revisión Vulkan y D3D12 offscreen

- HEAD/base65528d3528a3ba97d8fa94b0b1b1d5557e13deb4, main/origin Micchael710/Dlss.
  Sin reset/rebase. Diff funcionó con Git bundled; Git instalado fallaba.
- Run20261003-205509-501 intacto: gate0x170>=0x170 PASS, fixturePASS; Create0xBAD00002.
  Callback público confirma Kernel_BlendCandidatesFused / vkCreateCuModuleNVX=-3,
  VK_ERROR_INITIALIZATION_FAILED. Kernel FAIL_OBSERVED, causa internaUNKNOWN.
- Reducer diferencia NOT_OBSERVED/PASS_OBSERVED/FAIL_OBSERVED y emite reviews derivados.
  No segundo Vulkan: warning Width/Height compatible con helper oficial; sin error
  público host que explique rechazo. Sin ABI privada ni parche binario.
- Variante D3D12 pública al mismo harness: LUID fijo, fixture/readback/fences/warmup4,
  count1/index1, sin swapchain/Present. Compilada; única prueba20261003-214903-641.
- Loader/backend activos, NGXInitPASS. Evidence checkpoint failed y exit0xC0000409
  antes de fixture/Create/Evaluate. PRECONDITION/persistencia, no fallo FG probado.
  Eventos kernel auxiliares vacíos no prueban creación de nuestra feature.
- Recorder corregido (Win32/recovery/sin doble throw), compilación y tests CPU PASS,
  incluido bloqueo deliberado de checkpoint. No retryGPU. Estado final device removed
  UNKNOWN sin consulta final; no evidencia driver reset.
- Docs/reviewed-result por run; originales preservados. Baseline SR/Wisteria/AMD bridge
  hashes intactos. No Minecraft, runtime-baseline, perfiles, MFG ni sidecar.
- .gitignore mejorado para caches/transitorios y wrapper JAR local designorado upstream.
  Commit/push sólo fuentes/documentos/evidencia al repositorio del usuario.
- D3D12 x2 pendiente de demostrar; sidecar únicamente justificado para evaluación.

## 2026-10-03 — única prueba posterior al fix, D3D12 x2 offscreen PASS

- Base aacc6d26d5eb85e363d7bd4c946d2cda641dbbc0, main limpio al comenzar.
- Preflight: consulta pública final device removal y gates loader/backend añadidos
  por omisiones concretas; generación/recorder/arquitectura intactos. Build y CPU
  self-tests PASS, lock/recovery incluido, nuevo provenance CPU conservado.
- Único run20261003-222208-087: fixture/Create/Evaluate count1-index1/completion
  fence39/output temporal PASS. G1 b5261cc3c32720a2e4178611c9cb25d5cb964e4b554eeb6b449ec311f7993a3e,
  distinto A/B/sentinel, centro intermedio, generated_count_confirmed1.
- Device removedfalse/0, validation_errors0, worker exit0. Capabilities available/
  max1 reportadas potencialmente intervenidas, nunca prueba de generación.
- Clasificador CPU corregido separando seis diagnósticos vacíos y kernels reales:
  PASS_OBSERVED. Result original conservado, reviewed-result separado. IDs backend
  y enum NGX no equivalentes; regression test PASS; ninguna repetición GPU.
- Vulkan histórico no repetido; AMD/SR/Wisteria hashes intactos. No Minecraft,
  sidecar/interop nuevo, Dzn, ABI privada, cambios globales ni MFG superior x2.
- Diseño futuro de recursos/fences compartidos Vulkan↔D3D12 justificado; latencia
  por etapa pendiente, sin promesa zero latency. Parada tras documentación y
  push main sólo al repositorio autorizado del usuario.

## 2026-10-03 — probe cross-API, bloqueo CAPABILITY sin retry

- Base4c3efe98882e68e7b93039f1d868ea07655b7424; nuevo módulo aislado sin NGX.
- Build PASS, ejecución única Fase A20261003-223835-709. GPU Vulkan/DXGI UUID/
  LUID exactos RTX3050Ti. Shared resource/fence D3D12 Create/Open success.
- Vulkan RESOURCE features5 y HEAP4 importables, no exportables; FENCE2 import
  only en query binario. Gate de exportación nativa Vulkan falla, fases B–E
  NOT_RUN. No cinco variantes, no Dzn ni dispatch DLSS-G.
- Import-only no demuestra bridge global imposible. Origen allocation/fence
  D3D12 e importación Vulkan candidato futuro documentado, sin implementar.
- Debug errors0; Khronos layer ausente, limitación explícita. Device removed0.
  CPU wall2076.974ms sólo probe; tiempos GPU/handoff NOT_RUN, sin benchmark falso.
- ExitCode wrapper null preservado; cache handle añadido para futuro, sin retry.
- Baseline AMD/SR/Wisteria SHA256 intactos. No Minecraft, PresentWorker, Wisteria,
  driver/perfiles o MFG>x2. Docs/evidencia textual commit/push usuario y parada.

## 2026-10-03 — inversión de ownership, interop GPU-only PASS

- Base df13071b6cfd02fcbd501fcae1d0d0a09ef99e1f limpia. Nueva variante owned.cpp,
  sin NGX/DLSS-G, mantiene probe histórico sin invocarlo. Build Release x64/MT
  y CPU self-tests intersección/empty intersection/patrones PASS.
- Run único20261003-230013-062. RTX3050Ti UUID/LUID exactos. D3D12 recurso DEFAULT
  shared RGBA8/256x256/ALLOW_RENDER_TARGET, handle D3D12_RESOURCE importado Vulkan,
  dedicated allocation obligatoria, memory type por bits3&2=2/type1. Import/bind PASS.
- Clears GPU rojo D3D12 y azul Vulkan sobre la misma allocation; hashes completos
  exactos en readbacks del consumidor. No upload CPU funcional ni segunda texture.
- Fence D3D12 shared importado TIMELINE permanente, señales1..5; waits GPU entre
  queues, CPU sólo terminal de cada fase para validar y decidir continuación.
- Ambos sentidos y roundtrip PASS, transporte CPU0, validation readbacks2. Debug
  D3D12 errores0; warning820 de clear sin optimized clear value preservado. Vulkan
  Khronos ausente, debug-utils errores0; no device lost/removed. No benchmark GPU.
- Handles aplicación cerrados tras import; payload references vivos hasta completion.
  Protocol COMMON/GENERAL con ownership EXTERNAL y layouts transfer explícitos,
  observado en esta configuración sin afirmar equivalencia universal entre APIs.
- Baselines AMD/SR/Wisteria SHA256 intactos. No DLSS-G, Minecraft, PresentWorker,
  Dzn, ruta export Vulkan, HEAP fallback ni x3–x6. Docs/evidencia textual,
  commit/push usuario y parada antes de conectar recursos FG.

## 2026-10-03 — Vulkan → D3D12 DLSS-G → Vulkan x2 end-to-end PASS

- Base main/GitHub bd25bef3ea065b291a743837c140abfd77997bff, sin cambios iniciales.
  Nuevo harness aislado; reutiliza helpers públicos, fixture/reducer, loader y
  session D3D12 cerrados, sin editar sus sources ni ejecutar rutas históricas.
- Build Release x64/MT PASS. CPU reducer temporal/cámara/depth/checkpoint/intersección
  PASS; reducer de evidencia rechaza capabilities-only, kernels ausentes, duplicados,
  handoffs erróneos, CPU transporte y device loss. Un ajuste CPU antes de GPU;
  ninguna prueba GPU antes de pasar todos los checks.
- Única ejecución principal 20261003-233010-719, worker0/timeoutfalse. RTX3050Ti
  UUID01895b66d1ca454d88788dd21fdef638/LUID4c29010000000000, driver596.49.
- A/B/Depth/MV/G1 creados D3D12 DEFAULT/SHARED/UAV, importados Vulkan optimal
  dedicated D3D12_RESOURCE, formatos RGBA8/R32/RG32 históricos, resolution1280x720.
  Query exacta individual features5; memory bits3&2=2/type1 DEVICE_LOCAL, import/bind0.
- Fixture fuente CPU subida a Vulkan permitida por encargo §11, Vulkan GPU escribe
  allocations compartidas. NGX usa esos ID3D12Resource, Vulkan lee mismo G1.
  No GPU→RAM→GPU, segunda allocation de transporte, Dzn o traducción Vulkan.
- Fence D3D12 shared import timeline permanente. Intervalos0..4: señales impares
  inputs/pares completion; target9→9/10→10 GPU. No CPU wait entre APIs. Waits
  terminales tras consumidor Vulkan sólo para reciclar buffers/allocator/validación.
- Loader sdli0.3.5, runtime efectivo310.9.1 SHA ff6e90eb… idénticos al PASS previo.
  Public NGX Init/Create/Evaluate0x1, feature handle válido; 64 kernels reales
  PASS_OBSERVED, diagnósticos vacíos separados. Reported Available1/Max1 no prueba FG.
- Warmup0..3, reset sólo0, target4/count1/index1. G1 hashb5261cc3… distinto de
  A/B/sentinel; inputs exactos, disable0, centroid451.408675, MAE0.298147,
  backgroundBad0, generated_count_confirmed1. Reducer y preview G1 PASS.
- GPU scopes Vulkan input3.873824ms, DLSSG3.958784ms, Vulkan consumer/readbacks
  3.357536ms. Handoffs individuales UNKNOWN sin calibración común. TotalCPU
  5308.8086ms incluye setup/warmup/logging/validación, no latencia Minecraft.
- D3D12 debug errores0, warning1328 de initial state de buffer diagnóstico;
  Vulkan debug-utils0 y Khronos validation ausente. No device lost/removal/reason0.
  Readbacks CPU terminales9: seis contenidos, flag y dos scopes timestamps.
- VULKAN_DLSSG_D3D12_VULKAN_X2=PASS; DLSSG_VULKAN_SIDECAR_END_TO_END=PASS;
  MINECRAFT_INTEGRATION_READY=YES_FOR_NEXT_PHASE limitado al harness sintético.
- Baselines AMD/SR/Wisteria hashes antes/después intactos. No `.minecraft` real,
  retest Vulkan NGX, tests aislados, Wisteria/PresentWorker/ring fix, driver/DRS,
  OptiScaler, Streamline ni x3–x6. Evidencia textual/JSON y source propio,
  commit/push sólo origin del usuario y parada antes de integración Minecraft.

## 2026-10-03 — provider DLSS-G experimental persistente, antes de shadow

- Base 29deca27b6d4a4401699c83be4258a0fa02f10ab. Pipeline real auditado antes de
  modificar Wisteria: captura, snapshots, queues, output leases, PresentWorker,
  presupuesto de ocho slots conservador, extents y convenciones de motion/depth.
- Nuevo wisteria:dlssg / wisteria:dlssg_fg, APPLICATION_MANAGED_ASYNC, default OFF,
  shadow experimental por defecto y sin fallback automático AMD. JNI propio
  empaquetado; sdli0.3.5/runtime310.9.1 sólo staging local con hashes identificados,
  contrato público loader/INI y ningún export privado/componente comunitario en Git.
- Sesión D3D12/NGX persistente, D3D12-owned shared pool keyed, handles importados
  una vez, GPU copies4, output transport0, CPU image transport0. Fence timeline
  D3D12 permanente y GPU wait/signal entre APIs; setup wait acotado, no wait CPU
  por frame entre APIs. New optional submission plan/default-null mantiene AMD.
- G1/real son wrappers no propietarios de VkImages nativas; output fence cubre
  D3D12 completion y lease se recicla sólo después de presentación y generación.
  Resize/discontinuity/real gap invalidan historia; pools antiguos se drenan.
- C++/JNI Release x64 y candidatos Java21 PASS. Tests CPU cámara/depth/matrices/
  motion/jitter/inmutabilidad y ABI/linkage JNI PASS, sin GPU ni DLL comunitaria.
  Candidate hashes en dlssg-integration-artifact-manifest.json.
- Baseline SR/Wisteria/AMD native SHA256 idénticos; nueve clases AMD y sus dos DLL
  empaquetadas byte-for-byte iguales. Candidatos separados en builds/experimental.
- Diagnóstico acotado lee G1 desde el consumidor Vulkan. No pixel readback por
  frame; small output-disable flag/timestamps son metadata. Present callback sólo
  observa requests y queue delay, no cambia ownership/pacing. No display-latency
  claim sin baseline/control comparable.
- Minecraft shadow NOT_RUN; presentación generada NOT_RUN. Sin nuevos tests de
  gates cerrados, directo Vulkan NGX, DLSS SR, x3–x6, OptiScaler o .minecraft real.
  Primera integración acepta owned capture del perfil OpenGL FSR1; borrowed
  Vulkan-upscaler inputs requieren un join auditado antes de cambiar layouts.

## 2026-10-03 — single Minecraft DLSS-G shadow closed FAIL

Implementation milestone 621546f compiled/packaged/CPU ABI/metadata checks passed.
Single runtime-baseline shadow 20261004-010600-001 opened Minecraft1.21.1,
NeoForge21.1.219, preserved world/Complementary/SR1.7/OpenGLFSR1/Vulkan/OptiScalerOFF.
Actual final/HUDless/depthR32F/motionRGBA16F and immutable camera metadata observed.
Own JNI and public sdli0.3.5 loader loaded; UUID/LUID/driver match historical GPU.

First eligible job failed SESSION_INIT/D3D12_CREATE_DEVICE: HRESULT0x887A0007
DXGI_ERROR_DEVICE_RESET. NGX Init/CreateFeature/kernel/Evaluate/pool/sync not reached.
Accepted DLSS-G jobs0/generated0; init failures1; Evaluate failures0 means NOT_REACHED.
Provider latched unhealthy, FG=null, no automatic retry/AMD fallback. Minecraft stayed
alive; orderly close saved all dimensions and destroyed Vulkan. No presentation run.

Static hypothesis: reused standalone initializer activates process-wide D3D12 debug
layer late. Microsoft requires activation before device creation; a previous D3D12
device is not proven by loaded modules, so hypothesis medium confidence only.
Failing stage/HRESULT confirmed. No post-failure runtime fix/build/GPU rerun applied.
Next: separate embedded initializer from standalone debug bootstrap, then review
before a new authorized bounded runtime attempt. No private ABI or driver changes.

Original isolated config restored byte-for-byte. Three baseline SHA256s unchanged;
AMD candidate Java/native bytes identical. Earlier offscreen/AMD PASS gates preserved.
Evidence result/run-manifest/events/provider/capture/pool/sync/timing/error text saved
under logs/runtime/minecraft-dlssg-x2/20261004-010600-001. Binaries/UI screenshot/world
stay local/ignored. Higher MFG/DLSS SR/direct Vulkan NGX not tested. Stop at failed gate.

## 2026-10-03 — embedded D3D12 initialization correction, pre-run gates

Continue2898622. Explicit D3D12InitializationContext separates standalone debug
bootstrap from embedded Minecraft initialization. Embedded: no debug/DRED global
configuration, no D3D12GetDebugInterface/EnableDebugLayer; DXGI factory flags0.
Standalone default retains prior behavior; no closed GPU harness was rerun.
Same NVIDIA Vendor/LUID/name/non-software adapter, FL12_0 and public device IID;
DeviceId/flags and CreateDevice arguments/result now logged explicitly.

Bridge C++/JNI build PASS; Java21 builds PASS; 19 SR CPU tests,7 evidence tests and
DlssgContractTest matrices/depth/motion/jitter/metadata/reset/JNI ABI1 PASS.
AMD nine Java classes and two packaged DLLs identical; three baseline hashes unchanged.
Previous candidate JARs archived locally under builds/experimental/dlssg-x2/history/2898622.
New candidate manifest updated; previous failed runtime evidence remains untouched.
No changes AMD/PresentWorker/NGX/private ABI/loader/inputs/pool/scheduler/driver/DRS.
Next: exactly one new shadow, then one presentation x2 only if full shadow passes.

## 2026-10-03 — corrected-init Minecraft shadow PASS

Single shadow20261004-030130-357, implementationb87950e, no retry. Same UUID/LUID/
driver, public component/runtime hashes. Embedded debug NEVER enabled; DXGI flags0.
D3D12 device/queue, NGX Init/CreateFeature,64 named kernels, persistent pool8 images40,
shared fence,6776 Evaluate/GPU completions PASS.6775 non-reset generation-enabled
intervals;27104 Vulkan input blits; CPU image transport0/per-frame API CPU waits0.
Three Vulkan B/G readbacks31–33;32/33 verify G distinct from A/B/sentinel and scene/HUD
visually reasonable. First sample lacks A and supplies next sample anchor; not a fail.
PresentWorker callbacks real-only; no device lost/crash; drained/normal world save.
Config restored; AMD integrity PASS. Original failure run untouched. Raw image samples
stay local ignored. Static-world sample proof is distinct from broad motion quality.
Next automatically: one presentation x2 with identical candidates, no extra build.

## 2026-10-03 — presentation launched, USER_INTERRUPTION

Presentation20261004-031441-720 started once after shadow PASS, same candidate hashes.
Computer Use stopped by physical Escape while selecting windows, before agent world
load. No further UI/input/build/diagnostic/run. PENDING_USER_INTERRUPTION, not FAIL.
Resume same process/run; no shadow repetition or second presentation launch.
Isolated config remains staged until client shutdown; final commit/push pending.


## 2026-10-04 — frame4037 review and MFG preparation

HEAD inicial real: b87950e3f27e05e409efb7bc959599d901363c48. El usuario cerró
presentation20261004-033955-918; se conserva USER_INTERRUPTION / INCOMPLETE.
No se reanudó ese proceso, ni se lanzó otro x2/shadow. La continuación autorizada
prioriza x3 → x4 → x5 y deja DLSS SR para después.

Auditoría4037: NGX Evaluate SUCCESS, reset=false/epoch3/delta578.9245ms,
VkImage1695473108976, ready8069/done8070; Vulkan wait8070 y callback PRESENT
GENERATED para ese mismo ID/recurso. disable=1 leído antes de reutilizar ese slot.
El callback confirma petición de presentación aceptada, no scanout físico.
Bug demostrado: publicación de candidate G antes de leer su flag de GPU.
Sin readback de4037: corrupción/correctitud y causa del corte siguen UNKNOWN.

Corrección: evento de completion D3D12 con callback threadpool, snapshot inmutable
por índice, readiness de batch FIFO y filtro antes de adquirir/presentar target.
No espera de fence en dispatch/handoff, Sleep o polling. Cola presentation espera
notificación mediante su Condition ordinaria. Lifetime y semáforos de candidates
suprimidos se drenan al retirar el batch; no se destruyen recursos anticipadamente.
AMD conserva defaults síncronos y gate presentable; su backend no se modifica.

JNI ABI2, colección List<VulkanTexture> inmutable, N outputs por slot e indices1..N.
Inputs/frameID/count constantes dentro del grupo; reset sólo en index1 conforme
al contrato público ya auditado. El grupo entrega candidatos, nunca contadores
válidos por request. Válidos se cuentan tras completion + disable0 + !reset.
Pool=floor(6/(N+1))+2: x3=4slots/2outputs, x4=3/3, x5=3/4. El peso N+1 se
conserva incluso durante reset/discard, por lo que esa cota incluye esos grupos.

Capabilities: getter público real tras loader adaptado; puede estar intervenido.
El máximo solicitado en INI es independiente del valor reportado. Disponibilidad
no publicada hasta init/query; no getter propio modificado ni binarios parcheados.
Muestras: hasta3 intervalos, iniciadas con movimiento de cámara y sin reset.
Nuevos resultados al preparar la fase: todavía NOT_RUN; x4/x5 sólo después de PASS previo.

### 2026-10-04 — Java25 confirmado; primer x3 detenido

El usuario corrigió el runtime: Java25.0.4 requerido. Launcher aislado toolchain25;
auditRuntimeJava verifica el javaLauncher efectivo de runClient sin lanzarlo.
Los mods siguen con bytecode21 y sus builds previamente validados se reutilizan.

El único run x3 20261004-042043-949 se lanzó erróneamente con Java21.0.12.1.
Durante la compactación llegó a cargar mundo y terminó en crash; recuperación
de ventana encontró que ya estaba cerrado. No se envió otro input ni se reintentó.
NGX init/CreateFeature y64 kernel creates status0; 6 Evaluate/completion,
count2/indices1,2. Index2 flag -1 en los dos intervalos no-reset; error de
readiness `GPU disable metadata unavailable` propagado al presenter.
No readbacks; writer Java buffered y archivo vacío; present count/FPS/orden
UNKNOWN. No evidencia para atribuir el problema a Java21 ni para certificar
MFG o descartarlo en Java25. Stop x3; x4/x5 NOT_RUN; AMD hashes intactos.
Se preservan logs, resultado y reviewer offline conservador. No se tocan ABI
privada, drivers, .minecraft real, DLSS SR ni AMD.

## 2026-10-04 — DLSS Super Resolution: binding y packaging reales

Inicio HEAD5da0987 limpio. Auditoría de opción dlss existente: implementación
NGX Vulkan completa en gran parte, pero SR_NGX=OFF y DLL JNI ausente del JAR.
Log previo de extracción y carga real Java25 confirman recurso faltante.
Se habilitó build aislado SR_NGX_LIB con SDK público local, se incorporó su
DLL al JAR NeoForge experimental y se descargó runtime oficial firmado de
NVIDIA/DLSS commit374959484e79a640feaba44c93ac8cfb0a03f5b5 (310.9.1).
No se publica runtime propietario. Se agregaron comprobación capability,
handle válido, errores explícitos y diagnóstico de completions/output;
se eliminó retry automático de inicialización. Presets UI existentes mapeados
a enums NGX públicos. Build/CPU/JNI pasaron; AMD9clases/2DLL y clases FG y
presentación byte-identical. MFG histórico preservado, sin x3/x4/x5 nuevo.
Una prueba Minecraft DLSS+FG OFF iniciada `20261004-162909-911`; standalone
PASS aún pendiente de mundo/output/completion/exit. No combinación x2 hasta
que esta prueba pase. Ver DLSS_SUPER_RESOLUTION_INTEGRATION.md para inputs,
convenciones, hashes, limitaciones y evidencias de errores/build.

Corrección posterior: el arranque 20261004-162909-911 no ejecutó DLSS. El preparador duplicó CR al escribir TOML; NightConfig regeneró defaults FSR1/OPENGL. Evidencia conservada, no PASS ni fallo GPU DLSS. Tras cierre del cliente y petición de continuar, preparador corregido, presentación Vulkan explícita y preflight TOML válido. Prueba efectiva 20261004-170642-750 iniciada; resultado pendiente.

Resultado final 20261004-170642-750: FAIL antes de NGX. skip_init_vulkan=true conservado del config regenerado impidió RenderSystems.vulkan; createResources produjo NullPointerException. Evaluate/completion=0, output no demostrado. Usuario confirmó mundo/HUD y cierre normal solicitado; proceso salió NTSTATUS0xC0000409 tras guardar mundo. Causa inmediata de inicialización alta confianza; causa de fallo nativo de cierre pendiente. STOP: no resize/combinado/retry. Binding/packaging/build pasan, backend Minecraft NO validado. Disponibilidad del selector y prerequisito Vulkan quedan pendientes.

Usuario aportó captura: render de mundo reducido a esquina inferior izquierda y áreas negras extensas, HUD más ancho. Visual FAIL confirmado; no output DLSS ni PASS. Corrección explícita de la observación preliminar de terreno/HUD visibles.

## 2026-10-04 — Vulkan DLSS y teardown; standalone PASS, combinado FAIL
Desde eaca5f4 limpio. Policy startup DLSS resuelve Vulkan, selector usa prerequisitos/NGX reales; gate antes de resources. Teardown sólo de owners creados, Shutdown JNI device válido sólo tras Init exitoso; contexto oculto retenido hasta cierre. Builds/tests CPU/JNI PASS, AMD hashes intactos sin rerun. Standalone20261004-173230-747 PASS: Evaluate4421/completion4421, output854x480 usado por composite7, readbacks acotados no negros, usuario lista, cierre0. Combinado único20261004-173835-015-x2 FAIL: cameraViewToClip no invertible al construir constantes; FG Evaluate0/present G1=0, SR10492/completion10492, cierre0. STOP sin retry; origen de proyección pendiente. Config restaurada por hash. 0xC0000409 histórico no reproducido, causa histórica UNKNOWN. MFG evidencia y código x2 preservados. Ver DLSS_SUPER_RESOLUTION_INTEGRATION.md y nuevos runtime-result.json.

Corrección de causa combinada al completar evidencia: integration-failure.txt y DLSSG_EXPERIMENT_FAIL demuestran INPUT_MAPPING. El adaptador existente rechaza hasBorrowedAlgorithmInputs porque requiere queue join auditado. Native FG summary Create0/Evaluate0, clean exit. La matriz no invertible fue advertencia adicional, no causa demostrada del disable. Guard intacto; STOP sin retry. Próximo bloqueo de combinación: sincronización de inputs prestados, sin modificar MFG.
