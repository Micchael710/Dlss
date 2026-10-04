## Estado vigente — handoff experimental llega a FG Create/Evaluate; combinación x2 FAIL por device lost

Inicio b3dd23c0f0b28a9e494ea0aac61fb51209179f63, main limpio. Una sola
ejecución combinada: `20261004-181754-546-handoff-x2`, Java25.0.4,
MC1.21.1/Neo21.1.219, SR495x278/display854x480, count1/index1.
Build C++/JNI ABI3, SR Java21 y Wisteria PASS; contrato CPU rechaza13
casos inseguros. AMD9 clases/2DLL byte-identical, sin ejecución AMD.
La señal depth/motion ahora pertenece al submission SR real, después de
su último uso Vulkan. Receipt de frame/generación/device/queue/command/fence
obligatorio; native exige los semáforos en el wait del consumer. Family0,
producer index0 -> FG index1. Blit GPU a pool FG persistente, depthR32F y
motionRG16F->RG32F; source registra5->6->5. Esto NO es handoff GPU validado:
hay device lost y no se prueba el layout efectivo que deja NGX.
FG Create1, Evaluate2 SUCCESS; primer callback completion2/status1 RESET,
un REAL presentado, G1 presentado0. Segundo input vkQueueSubmit falla -4
(VK_ERROR_DEVICE_LOST). STOP sin retry. SR Evaluate4, completion events0.
El guard de lifetime impide reutilizar un slot unrecoverable y termina en
crash Java; cliente exit-1/Gradle1, cierre normal NO. No 0xC0000409 observado.
Causa del gate demostrada; causa subyacente GPU/ordering/layout UNKNOWN.
Advertencia de proyección Infinity en frame2615 preservada sin fallback;
frames siguientes sí llegan a FG, por lo que no es el gate primario.
Config restaurado byte-for-byte SHA256 ade8502bc8d5d523397b5e1838e1f9100d9782ff116c12ef7484594095172462.
DLSS SR standalone sigue PASS histórico y NO se repitió. MFG x3 evidencia
intacta; x3/x4/x5/x6 no ejecutados. Detalles en DLSS_SR_DLSSG_X2_INTEROP.md.

## Estado anterior preservado — DLSS SR standalone PASS; combinación x2 FAIL antes de FG Evaluate

Iteración desde eaca5f48dfd47b217cec84d5d2350aeec3d3e052. La selección
startup dlss resuelve Vulkan aunque skip_init_vulkan sea true; FSR/OpenGL
conserva su política. Gate explícito de device/GL interop, disponibilidad
NGX real en selector, teardown sin acceder a recursos no creados y JNI
Shutdown sólo tras Init exitoso. Código0 en las dos sesiones de esta fase.
Build C++/Java y tests CPU/JNI mínimos PASS. AMD9 clases/2DLL hashes intactos.
Standalone20261004-173230-747: Java25.0.4, FG OFF, Init/capability/Create
result1, handle válido, Evaluate4421/completion4421, render495x278,
output854x480 usado como SR_output93 -> autotex3 target66 -> composite7
viewport854x480. Tres readbacks sin píxel RGB negro, imágenes distintas
y diferentes de input nearest; mundo completo. Cierre0 sin device lost.
Combinado20261004-173835-015-x2: SR Evaluate10492/completion10492, pero
adaptador FG bloqueó INPUT_MAPPING: inputs prestados por upscaler Vulkan
requieren queue join validado. FG Create0/Evaluate0/generated presents0;
combinación FAIL. También hubo advertencia de matriz no invertible, que no
es la causa demostrada de disable. STOP sin retry. Cierre0, config restaurado.
El 0xC0000409 histórico NO se reprodujo; su causa exacta sigue UNKNOWN.
MFG/AMD/implementación DLSS-G x2 histórica intactas. No x3/x4/x5 nuevos.
Detalles y evidencia: docs/DLSS_SUPER_RESOLUTION_INTEGRATION.md.

## Estado anterior preservado — fallo antes de NGX

Inicio5da09874956956b04a5f413999a5c4217235c6a2. No MFG nuevo.
La opción existente dlss usa NGX Vulkan. El native faltaba en el JAR;
SR_NGX=OFF y SDK submodule vacío. Ahora SR_NGX_LIB se construye aislado,
se empaqueta la DLL propia y se usa runtime SR oficial NVIDIA firmado.
No sidecar SR D3D12, no segundo presenter, no FSR bajo nombre DLSS.
Capability/handle se comprueban realmente; no retry automático ni output
silencioso cuando Evaluate/context falla. Presets existentes se mapean a NGX.
Build Java21/native/JNI load y testsCPU pasaron. AMD9clases/2DLL y clases
FG/presentation idénticas. Arranque `20261004-162909-911` descartó el TOML
por CR duplicado en el preparador; seleccionó defaults FSR1/OPENGL. No
alcanzó DLSS y no cuenta como evidencia de su runtime. Cliente cerrado.
Preparador corregido: normalización de CR y escritura de bytes, presentación
VULKAN explícita. TOML validado antes del arranque `20261004-170642-750`:
Java25.0.4, DLSS Balanced ratio1.724, FG OFF. Runtime FAIL: debug
skip_init_vulkan=true dejó RenderSystems.vulkan() null; la opción DLSS
intentó crear recursos y falló antes de NGX. Evaluate/completion=0.
Mundo/HUD visibles según usuario, sin output DLSS demostrado. Cierre pedido
normalmente, pero proceso terminó NTSTATUS0xC0000409. STOP: no resize,
no combinación x2, no otro intento. Próximo trabajo: corregir prerequisito
Vulkan y disponibilidad del selector, y diagnosticar cierre sin atribuirlo
a NGX. Native/build no equivalen a backend validado en Minecraft.
Detalles: DLSS_SUPER_RESOLUTION_INTEGRATION.md. No lanzar segunda sesión
ni combinación antes de cerrar esta prueba con evidencia completa.

## Estado preservado — G2 produce imagen; presentación x3 bloqueada, 2026-10-04

HEAD inicial de esta fase: 91c8292b560b4e87bd3a8d4a9fac7ab45880e201.
Diagnóstico único `20261004-151654-208`, Java25.0.4, count2/indices1,2.
Cuatro grupos capturados572–575, tres pares A/B anclados y sin reset.
G2 es distinto de A/B/G1, sin sentinel, no negro. Estimación de movimiento
A<G1<G2<B en las tres muestras, con confianza moderada y artefactos de nubes.
G2_IMAGE_GENERATION=PASS_BOUNDED_DIAGNOSTIC; G2_OPTIONAL_STATUS=UNKNOWN.
Esto sustituye cualquier inferencia de que sentinel de estado implica ausencia
de imagen. NO autoriza G2: cero presentaciones de index2.

Contrato público: output disable opcional, hint boolean en primer byte de
buffer>=4 bytes para frame(s) interpolado(s). Sugiere intención de grupo pero
no demuestra que flag0 de G1 se pueda propagar a G2. No se cambió el gating.
MINECRAFT_DLSSG_X3=BLOCKED_STATUS_SEMANTICS_UNPROVEN; funcional PASS=NO.
Sin retest de presentación, sin x4/x5. Getter2 corresponde también al techo
configurado MaxGeneratedFrames2, no prueba un límite intrínseco de la ruta.
Streamline/OptiScaler permiten solicitar count, pero administran presentación;
no contrato público demostrado de exportar outputs manteniendo PresentWorker.

Normal shutdown, sin crash/device lost; AMD9clases/2DLL idénticos. Modo
diagnóstico apagado por defecto, warmup30, máximo4 muestras/600dispatches.
Readback sólo diagnóstico, transporte CPU de gameplay0, único PresentWorker.
Informe y fuentes: `DLSSG_MFG_PUBLIC_CONTRACT_IMAGE_DIAGNOSTIC.md`.
DLSS_SR_NEXT_ITERATION_REQUIRED=YES; DLSS_SR_UI_OPTION_ALREADY_EXISTS=YES.
La siguiente iteración debe implementar NGX DLSS SR real en la opción existente.

## Estado histórico preservado — auditoría index2, 2026-10-04

HEAD inicial ed7a4d4; trabajo pendiente Java25 conservado en commit2cd78d3.
Prueba única nueva x3: `20261004-144431-183`, Java25.0.4 desde Minecraft,
count2/index1,index2. FAIL_OUTPUT2_STATUS_NOT_WRITTEN. Getters NGX y backend
externo confirman index2/count2 y SUCCESS; recursos y estados independientes.
Sentinel leído antes4294967295 y después4294967295 para G2, también al final
completo del grupo. G1 cambia a0. GPU group completion observada. UNKNOWN
se descarta, nunca se convierte a disable válido ni se presenta como G2.
Última frontera: NGX Evaluate y completion GPU del grupo. Causa interna
indeterminada; no atribuir a Java, RTX3050Ti ni componente externo sin evidencia.
Builds/tests locales pasaron, AMD9 clases/2DLL idénticos al baseline.
Sin retry/x4/x5/multiplayer. Mantener fix PresentWorker, FSR1 y bytecode21.
Detalles y veinte comprobaciones en `DLSSG_INDEX2_PIPELINE_AUDIT.md`.

## Estado vigente — runtime Java 25; x3 detenido por metadata de output

El usuario confirmó que Minecraft debe ejecutarse con Java 25. El launcher aislado
selecciona `C:/Program Files/Java/jdk-25.0.4/bin/java.exe`; los mods precompilados
conservan bytecode Java 21. Esto sustituye las instrucciones históricas de ejecutar
Minecraft con Java 21. No se recompilaron los mods ni se cambiaron drivers.

Run x3 `20261004-042043-949`: se lanzó por error con Java 21.0.12.1. Al recuperar
el estado después de la compactación, ya había cargado el mundo y había terminado
con un crash Java. Se preserva como FAIL de ese run, **no** como resultado de Java25.
No atribuir el fallo a la versión de Java: la relación causal sigue UNKNOWN.

NGX init/CreateFeature PASS, 64 eventos de creación de kernels con status0;
3 intervalos/6 Evaluate SUCCESS y 6 completions. Count2/indices1,2; recursos
distintos. Flags: index1=[1,0,0], index2=[1,-1,-1]. La readiness rechazó el
segundo output (`GPU disable metadata unavailable`); no se aprobó MFG.
El valor -1 coincide con el sentinel inicial 0xffffffff. El snapshot mixto
[0,-1] excluye el catch-all de lectura/fence (ese camino pondría todos los
flags a -1). No se ha probado por qué el segundo flag conserva ese valor.

No hubo readbacks. El writer Java no había vaciado su buffer cuando ocurrió el
crash: frame-sequence.log quedó vacío. Present count/order/FPS/quality UNKNOWN
o NOT_MEASURED. No inferir cero presentaciones de ese archivo vacío.
El contador nativo llegó a 2 outputs con disable0/completion/non-reset, pero
ninguno tiene validación de contenido. DeviceRemoved=false/reason0 observado
en D3D12; no error Vulkan device-lost observado. AMD hashes siguen intactos.

Resultado: `logs/runtime/minecraft-dlssg-x3/20261004-042043-949/result.json`.
x3 con Java25 NOT_RUN; x4/x5 NOT_RUN conforme al stop obligatorio. No rerun
shadow/x2, no DLSS SR, no ray tracing, no x6. Próxima fase: auditar el contrato
público del flag MFG y recuperar observabilidad antes de autorizar otra prueba.

## Preparación previa — gating de output y MFG

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
Nuevos resultados: todavía NOT_RUN; x4/x5 sólo después de PASS previo.

## Pausa por USER_INTERRUPTION — Escape físico, no fallo técnico

Shadow20261004-030130-357 cerrado PASS; pruebas y fix compilado en b87950e.
Presentation20261004-031441-720 ya lanzado una vez; Computer Use interrumpido
antes de seleccionar ventana/cargar mundo por el agente. Validación PENDING.
No nueva ejecución: continuar el mismo proceso/run cuando el usuario lo indique.
No diagnóstico extra, no input tras Escape, no retest shadow ni gates históricos.
Config aislada DLSS-G sigue preparada; restaurar sólo tras cerrar este cliente.
Evidencia y documentación shadow guardadas; commit/push final pendientes.

## Estado vigente — nuevo shadow DLSS-G PASS; presentación x2 siguiente

Run `20261004-030130-357`: D3D12CreateDevice S_OK, debug no habilitado por nuestro
inicializador embedded; GPU exacta, NGX Init/CreateFeature,64 kernels named PASS.
Pool8/40 images persistentes, fence importada;6776 Evaluate/submits/completions,
6775 intervalos non-reset con disable flag0. Copias GPU inputs27104 (4/job), CPU0.
Tres readbacks Vulkan; muestras32/33 con A conocido, G distintoA/B/sentinel,
mundo/HUD completos sin corrupción obvia; muestra31 usada como ancla A.
Sólo real presentations; no retry/crash/device lost, drain y shutdown normales.
Dos muestras validan contenido; contador6775 no es hash de cada output.
Config aislada restaurada y hashes baseline iguales. Presentación: aún NOT_RUN,
ahora autorizada automáticamente por gate PASS, una sola ejecución x2.

Nueva fase autorizada desde `289862295b04422a41dc0a5a47068ab1b00c9bd4`.
Inicializador D3D12 distingue StandaloneHarness (debug antes de su device) de
EmbeddedMinecraft (no consulta/enable/disable debug ni DRED; DXGI factory flags0).
Bridge llama explícitamente EmbeddedMinecraft. Registra debug no disponible para
este inicializador; no afirma conocer la configuración previa de terceros.
Selección NVIDIA/LUID/no software y CreateDevice FL12_0/IID_ID3D12Device no cambian.

Build C++/JNI y Java21 PASS; 19 tests CPU SR, 7 tests CPU de evidencia y
DlssgContractTest (matrices/depth/motion/jitter/metadata/reset/JNI ABI1) PASS.
Packaging confirma nueve clases AMD y dos DLL AMD idénticas; hashes baseline
sin cambios. Candidatos de fase previa archivados localmente antes de empaquetar.
Siguiente: un único nuevo shadow; si pasa, una presentación x2; si falla, stop.
No cambios PresentWorker/AMD/NGX/inputs/scheduler; no nuevos harnesses offscreen.

## Fase previa — único shadow Minecraft FAIL; preservado

Run `20261004-010600-001` (UTC; 2026-10-03 local), implementación `621546f`.
Minecraft1.21.1/NeoForge21.1.219 abrió el mismo mundo con Complementary, SR1.7,
OpenGL FSR1, Vulkan presentation y OptiScaler OFF. Captura real confirma color,
HUDless, depthR32F, motionRGBA16F y metadata inmutable. JNI propio y loader público
sdli0.3.5 cargaron; GPU UUID/LUID y driver596.49 coinciden con la baseline.

`SESSION_INIT/D3D12_CREATE_DEVICE` falló con `0x887A0007` (`DXGI_ERROR_DEVICE_RESET`)
antes de NGX Init, CreateFeature, kernel creation, pool o Evaluate. G1=0. Provider
quedó unhealthy sin retry, negociación FG=null, sin cambiar silenciosamente a AMD.
Cliente continuó mostrando mundo/GUI y cerró guardando todas las dimensiones,
destruyendo Vulkan y finalizando launcher BUILD SUCCESSFUL. Eso no es PASS de FG.

Hipótesis de causa: el helper del harness standalone activa EnableDebugLayer durante
la inicialización tardía del provider en un proceso gráfico ya activo. Microsoft
documenta que activarla tras crear un dispositivo D3D12 lo retira. La activación
tardía está verificada en fuente; existencia de un dispositivo D3D12 previo y causa
última del reset siguen sin demostrarse. No se aplicó corrección ni segundo ensayo.

`MINECRAFT_DLSSG_SHADOW_X2=FAIL`; presentación `NOT_RUN_SHADOW_GATE_FAILED`.
Tres gates offscreen y AMD x2 conservan sus PASS históricos; NO se repitieron.
Hashes baseline/JARs/nativo AMD siguen idénticos. Config aislada restaurada al SHA
original. No .minecraft real, x3–x6, DLSS SR, Dzn, nuevos presenters o perfiles.
Evidencia: `logs/runtime/minecraft-dlssg-x2/20261004-010600-001/result.json`.
Siguiente fase: separar bootstrap debug standalone de inicialización embebida;
revisar evidencia antes de autorizar otro ensayo acotado. Provider default OFF.

## Hito anterior de esta fase — candidato Wisteria DLSS-G preparado

Continuación desde `29deca27b6d4a4401699c83be4258a0fa02f10ab`. Auditoría del pipeline
real escrita antes de cambiar Wisteria en CURRENT_REAL_PIPELINE.md. Provider
experimental `wisteria:dlssg`, APPLICATION_MANAGED_ASYNC, default OFF y shadow por
defecto. Sesión D3D12/NGX y pool D3D12-owned de ocho slots persistentes, cuatro
copias GPU de inputs, G1 importado directo, fence timeline compartida permanente.
Nueva submission plan consume readiness una vez; espera GPU Vulkan->D3D12->Vulkan,
sin espera CPU por frame entre APIs. PresentWorker conserva adquisición/presentación.

C++/JNI, Java21, empaquetado y tests CPU de cámara/depth/motion/matrices/immutabilidad/
ABI propia PASS. Nueve clases AMD y DLLs AMD incluidas idénticas a la baseline.
JARs candidatos separados de builds/baseline; hashes en
logs/runtime/dlssg-integration-artifact-manifest.json. Sólo captura owned del perfil
OpenGL FSR1 en esta primera integración; inputs borrowed requieren join adicional.
En ese hito aún no existía ejecución Minecraft del candidato. El único shadow
posterior falló según el estado vigente de arriba. Sin retest
de gates cerrados, DLSS SR, OptiScaler, x3–x6 ni modificación de .minecraft real.

## Estado anterior — Vulkan → DLSS-G D3D12 → Vulkan x2 PASS, 2026-10-03

Run único `20261003-233010-719`, base `bd25bef3ea065b291a743837c140abfd77997bff`.
Build y CPU tests PASS antes de una sola ejecución GPU. RTX3050Ti UUID/LUID exactos,
driver596.49, mismos sdli0.3.5/runtime310.9.1 y contrato x2 previamente validado.
Cinco allocations D3D12 DEFAULT/SHARED/UAV importadas Vulkan, dedicadas, type1
individual por bits3&2=2: A/B RGBA8, depthR32, MVRG32, G1RGBA8. Vulkan GPU escribe
fixture, D3D12 NGX genera G1, Vulkan consume el mismo G1; transporte CPU0.
Fence timeline permanente, GPU wait/signal target9→9/10→10, warmup0..3 coherente.
NGXInit/CreateFeature/64 kernels reales/Evaluate/completion/readback temporal PASS;
capabilities Available1/Max1 no son prueba. G1 distinto A/B/sentinel, centroid
451.408675 y MAE0.298147. Generated_count_confirmed1. No device lost/removal,
debug D3D12 errores0, Vulkan observados0 con Khronos layer ausente.
GPU target inputs3.873824ms, DLSSG3.958784ms, readbacks3.357536ms; handoff UNKNOWN
sin common-clock, wallclock5308.8086ms incluye setup/warmup/validación.
VULKAN_DLSSG_D3D12_VULKAN_X2=PASS; DLSSG_VULKAN_SIDECAR_END_TO_END=PASS;
MINECRAFT_INTEGRATION_READY=YES_FOR_NEXT_PHASE limitado al harness offscreen.
Baselines AMD/SR/Wisteria hashes intactos. No retest de los gates aislados/Vulkan
directo, Minecraft, Wisteria runtime, PresentWorker, Dzn, CPU bridge o x3–x6.
Fase detenida tras evidencia/docs y commit/push autorizado al repositorio del usuario.
Informe [DLSSG_VULKAN_D3D12_X2_END_TO_END.md](DLSSG_VULKAN_D3D12_X2_END_TO_END.md).

## Estado histórico — D3D12-owned Vulkan interop PASS, 2026-10-03

Run único20261003-230013-062: resource/fence creados por D3D12, Vulkan importa.
D3D12_RESOURCE exacto RGBA8/256x256/RT, IMPORTABLE+DEDICATED_ONLY. Intersección
memoryTypeBits3&2=2, type1 DEVICE_LOCAL; allocation dedicada/bind PASS.
Misma allocation: D3D12 clear rojo → Vulkan readback exacto; Vulkan clear azul
→ D3D12 readback exacto. Transporte CPU0; readbacks de validación2.
Fence D3D12 importado timeline permanente; signal/wait1,2,3,4,5 GPU ordenados.
Sync ambos sentidos PASS. D3D12_OWNED_VULKAN_INTEROP=PASS; CROSS_API_INTEROP=PASS;
DLSSG_VULKAN_SIDECAR_PREREQUISITES=PASS limitado a color/sync de este harness.
D3D12 debug errores0; Vulkan observados0, Khronos validation ausente. No device
lost/reason0. Sin rerun de DLSS-G/Vulkan export/HEAP; sin Minecraft/MFG>x2.
Baseline AMD/SR/Wisteria hashes intactos, PresentWorker y bridge existente intactos.
Fase detenida antes de conectar inputs/outputs DLSS-G.
Informe [VULKAN_D3D12_INTEROP_AUDIT.md](VULKAN_D3D12_INTEROP_AUDIT.md).

## Estado histórico — bridge cross-API detenido en capability, 2026-10-03

Run20261003-223835-709, sólo probe Vulkan/D3D12, sin NGX/DLSS-G/Minecraft.
SAME_GPU=PASS RTX3050Ti UUID/LUID exactos. RESOURCE features5/HEAP4/FENCE2:
importables en Vulkan, no EXPORTABLE. Ruta literal Vulkan-native-export→D3D12
no supera gate. CROSS_API_INTEROP=FAIL_CAPABILITY; shares/sync/roundtrip NOT_RUN.
No otra variante automática. Import-only no prueba inviabilidad de todo bridge:
allocation/fence D3D12 importados en Vulkan quedan como candidato sin probar.
DLSSG_VULKAN_SIDECAR_PREREQUISITES=NOT_READY. DLSSG_SM86_D3D12_X2=PASS histórico
intacto; no Vulkan DLSS-G retest. Debug D3D12 errores0, Vulkan observados0 con
Khronos layer ausente. Baselines AMD/SR/Wisteria hashes conservados.
No Wisteria/PresentWorker/bridge existente modificado ni MFG>x2. Parada.
Informe [VULKAN_D3D12_INTEROP_AUDIT.md](VULKAN_D3D12_INTEROP_AUDIT.md).

## Estado histórico — D3D12 DLSS-G SM86 x2 offscreen PASS, 2026-10-03

Una sola ejecución nueva20261003-222208-087, RTX3050Ti LUID4c29010000000000,
driver596.49, sdli0.3.5/runtime310.9.1 identificados. Loader/backend/NGXInit/
fixture/CreateFeature/Evaluate/completion/output temporal PASS. G1 distinto
A/B/sentinel; generated_count_confirmed1. Device removedfalse/reason0,
validation_errors0. Capabilities available1/max1 no son prueba de FG.
Kernels reales PASS_OBSERVED; diagnósticos vacíos separados en reviewed-result,
result.json original conservado. Recorder CPU tests PASS; sin otra prueba GPU.
DIRECT_VULKAN_X2=FAIL_AT_KERNEL_CREATION histórico; VULKAN_RETESTED=NO.
SIDECAR_ARCHITECTURE_JUSTIFIED=YES para diseño futuro GPU shared memory/fences
Vulkan↔D3D12, sin traducción completa ni RAM roundtrip. Bridge NOT_IMPLEMENTED.
No Minecraft, x3–x6 ni modificación AMD/SR/Wisteria/PresentWorker/baseline.
Detalle [DLSSG_D3D12_X2_RESULT.md](DLSSG_D3D12_X2_RESULT.md).
Fase terminada: evidencia/documentación, commit/push autorizado usuario y parar.

## Estado histórico — Vulkan revisado y D3D12 offscreen intentado, 2026-10-03

HARNESS BUILD=PASS; loader=LOADED; backend=OBSERVED ACTIVE; NGX Vulkan Init=PASS.
Run Vulkan20261003-205509-501 preservado: ARCH_GATE=PASS (0x170>=0x170), fixturePASS.
CREATEFEATURE=FAIL0xBAD00002, KERNEL_CREATE=FAIL_OBSERVED: Kernel_BlendCandidatesFused,
vkCreateCuModuleNVX=-3 VK_ERROR_INITIALIZATION_FAILED. IF_FAIL_STAGE=kernel creation.
Causa internaUNKNOWN; Evaluate/outputNOT_RUN, generated0. DIRECT_VULKAN_X2=
FAIL_AT_KERNEL_CREATION sólo para RTX3050Ti/596.49/310.9.1/sdli0.3.5 del run.
SECOND_VULKAN_RUN_PERFORMED=NO; no error público host que justifique otro intento.

D3D12 offscreen implementado y compilado, una ejecución20261003-214903-641,
LUID4c29010000000000, NGX_INIT=PASS, sin swapchain. Antes de fixture/Create/Evaluate:
Evidence checkpoint failed; salida0xC0000409. Clasificación PRECONDITION/persistencia,
no fallo demostrado FG. Kernel_create diagnósticos vacíos no son nuestra feature.
DLSSG_SM86_D3D12_X2=FAIL de run incompleto; generación D3D12 aún sin demostrar.
Recorder corregido/recompilado/test lock-recovery sóloCPU PASS. Ningún retryGPU.
NEED_D3D12_SIDECAR=YES_FOR_EVALUATION; SIDECAR_ARCHITECTURE_JUSTIFIED=NO.
AMD_FSR_FG_X2=PASS intacto, SHA256 baseline conservados. No Minecraft/sidecar/MFG
ni modificación de mods/PresentWorker/interop/runtime-baseline. ABI privada fuera.
Review [DLSSG_VULKAN_X2_RUN_REVIEW.md](DLSSG_VULKAN_X2_RUN_REVIEW.md).
Resultados originales intactos; reviewed-result.json separados por run.

## Estado anterior — harness externo diseñado, 2026-10-03

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

Corrección visual de esta fase: captura del usuario muestra mundo sólo en
rectángulo inferior izquierdo y áreas negras arriba/derecha. Calidad visual
FAIL; no se acepta mundo visible como salida válida de DLSS.
