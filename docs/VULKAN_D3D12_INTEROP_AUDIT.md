# Vulkan/D3D12 — interop y DLSS-G x2 end-to-end PASS, 2026-10-03

## Resultado vigente — Vulkan → DLSS-G D3D12 → Vulkan

Run único `20261003-233010-719`, base `bd25bef3ea065b291a743837c140abfd77997bff`:
**VULKAN_DLSSG_D3D12_VULKAN_X2=PASS**, **DLSSG_VULKAN_SIDECAR_END_TO_END=PASS**.
Misma RTX3050Ti/UUID/LUID/driver596.49, sdli0.3.5 y runtime310.9.1. Se reúnen
los dos PASS previos sin repetirlos por separado ni reintentar NGX Vulkan directo.

Cinco textures1280x720 D3D12 DEFAULT/SHARED/UAV, importación D3D12_RESOURCE
dedicada por recurso: A/B/G1RGBA8, depthR32, MVRG32; usage Vulkan transfer/sampled/
storage15, features5, bits3&2/type1. Vulkan escribe fixture, D3D12 NGX x2 procesa
esos mismos recursos y Vulkan lee el mismo VkImage G1. GPU handoffs timeline
permanente, target signal/wait9/9 y10/10; estados/ownership explícitos. Transporte
CPU0, readbacks terminales9 incluyendo flags/timestamps; waits CPU de reciclaje
sólo después del consumidor Vulkan. No segunda texture de transporte ni Dzn.

NGXInit/CreateFeature/64 kernels reales/warmup/Evaluate/completion/output temporal
PASS. G1 distinto A/B/sentinel y temporalmente intermedio; input readbacks exactos.
No device lost, D3D12 debug errores0, Vulkan debug-utils0 con validation layer
ausente. DLSSG target GPU3.958784ms, Vulkan input3.873824ms/consumer3.357536ms;
handoff individual UNKNOWN sin clock común. NO sumar clocks ni inferir FPS.

`MINECRAFT_INTEGRATION_READY=YES_FOR_NEXT_PHASE` sólo habilita el diseño futuro de
interfaz aislada Wisteria; no se integra aquí. AMD x2, Wisteria, PresentWorker,
ring fix, GL/Vulkan y `.minecraft` real intactos. No MFG>x2.
Detalle/alcance: [DLSSG_VULKAN_D3D12_X2_END_TO_END.md](DLSSG_VULKAN_D3D12_X2_END_TO_END.md).

## Resultado histórico — una allocation, uso GPU bidireccional

**D3D12_OWNED_VULKAN_INTEROP=PASS**, run único `20261003-230013-062`,
base `df13071b6cfd02fcbd501fcae1d0d0a09ef99e1f`. D3D12 crea/exporta y Vulkan
importa: esta inversión de ownership evita la exportación Vulkan no soportada.
No se repitió aquella ruta, Vulkan DLSS-G directo ni D3D12 DLSS-G x2.

RTX3050Ti Laptop GPU en ambas APIs, UUID01895b66d1ca454d88788dd21fdef638,
LUID4c29010000000000. Recurso256x256 RGBA8 UNORM, mip1/layer1/sample1,
D3D12 DEFAULT/SHARED/ALLOW_RENDER_TARGET, inicial COMMON. Vulkan optimal,
usage TRANSFER_SRC|TRANSFER_DST|COLOR_ATTACHMENT. Query exacta devuelve
IMPORTABLE+DEDICATED_ONLY features5, compatibleHandleTypes64. No HEAP fallback.
No CPU upload de píxeles finales; clears GPU en ambas APIs.

Vulkan image memoryTypeBits3, imported HANDLE bits2, intersección2; type1 elegido
por compatibilidad y DEVICE_LOCAL. Memory requirement262144 bytes.
VkExternalMemoryImageCreateInfo(D3D12_RESOURCE), allocation pNext:
VkImportMemoryWin32HandleInfoKHR → VkMemoryDedicatedAllocateInfo(image).
AllocateMemory/BindImageMemory PASS. Mismo ID3D12Resource y VkImage asociado
permanecen vivos hasta completion; no segunda allocation de imagen funcional.

| Prueba | Referencia del clear GPU | Readback consumidor |
|---|---|---|
| D3D12 rojo → Vulkan | b8b9b478324141d9331328a2150e74ec5feb266a743a51cab73d903247247ea4 | idéntico |
| Vulkan azul → D3D12 | 592222441a1005a084f94ce26ec7db665f43c10cc6c373e19544774cbdb0c5eb | idéntico |

Hashes de referencia calculados CPU independientemente para el resultado esperado
del clear; nunca se suben esos bytes a la texture. Verificación Python comprueba
todos los262144 bytes de cada readback, no sólo hashes. SOURCE_HASH significa
referencia determinista del productor GPU. CROSS_API_INTEROP=PASS.
DLSSG_VULKAN_SIDECAR_PREREQUISITES=PASS en el alcance color/sync de este harness;
formatos depth/MV/storage, buffers FG y pacing siguen pendientes de la siguiente fase.

## Sincronización, states y ownership observados

Fence creado D3D12_FENCE_FLAG_SHARED e importado en Vulkan como TIMELINE,
flags0/permanente. Query timeline exacta devuelve IMPORTABLE features2;
feature timelineSemaphore habilitada. VkTimelineSemaphoreSubmitInfo expresa
valores; no se necesita exportar semáforo desde Vulkan.
[Khronos recomienda timeline para fences D3D12](https://docs.vulkan.org/refpages/latest/refpages/source/VkD3D12FenceSubmitInfoKHR.html).

1. D3D12 COMMON→RENDER_TARGET, clear rojo, →COMMON, queue Signal1.
2. Vulkan submission Wait1; acquire EXTERNAL/GENERAL→graphics/TRANSFER_SRC_OPTIMAL,
   copy al buffer de validación, barrier TRANSFER_WRITE→HOST_READ del buffer,
   release image →EXTERNAL/GENERAL, Signal2.
3. D3D12 queue Wait2, Signal3. CPU espera sólo al final de esta prueba para
   validar A antes de permitir la siguiente prueba; no ordena producer/consumer.
4. Vulkan submission Wait3; acquire GENERAL→TRANSFER_DST_OPTIMAL, clear azul,
   release →EXTERNAL/GENERAL, Signal4.
5. D3D12 queue Wait4; COMMON→COPY_SOURCE, copy al readback de validación,
   →COMMON, Signal5. CPU espera completion terminal para comprobar B.

Ningún Sleep/polling ni WaitForSingleObject entre producer y consumer como
ordenación principal. Todas las esperas CPU acotadas15s son terminales de
validación; cada handoff real usa waits GPU. Se abortaría tras hash A inválido,
sin ejecutar B. La misma allocation R se mantiene en ambas pruebas.

COMMON y GENERAL son el protocolo elegido/observado en esta configuración,
con barriers de estados y ownership explícitos. No se afirma que ambos trackers
sean equivalentes ni que la combinación garantice compatibilidad universal.
La tabla Vulkan de layouts implícitos externos incluye D3D11, no D3D12;
esta distinción se conserva. [La especificación describe ownership externo y
aliasing entre APIs](https://docs.vulkan.org/spec/latest/chapters/resources.html),
y [Microsoft exige estados/barriers explícitos](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12).

| HANDLE | Creador | Importador | Import consume handle | Cierre |
|---|---|---|---|---|
| Recurso NT | D3D12 CreateSharedHandle | Vulkan AllocateMemory | No | Aplicación tras import exitoso |
| Fence NT | D3D12 CreateSharedHandle | Vulkan ImportSemaphore | No | Aplicación tras import permanente exitoso |

Sin DuplicateHandle ni double-close. Ambas APIs retienen payload references.
[Khronos especifica que importar memoria Win32 no transfiere ownership del handle](https://docs.vulkan.org/refpages/latest/refpages/source/VkImportMemoryWin32HandleInfoKHR.html).
Import/bind y timeline/permanence están en logs y handle-ownership.json.

## Alcance del PASS y parada

SAME_ALLOCATION_SHARED=YES. Transporte CPU0, GPU copies de transporte0.
Dos GPU copies y dos mappings CPU únicamente para readbacks de validación;
los clears funcionales no necesitan copia entre APIs. No etiqueta zero-copy
global ni promesa de latencia cero. CPU wall total1160.5751ms incluye drivers,
resources, logs, hashes, validación y cleanup; no mide tiempo GPU/handoff.
Sin timestamps GPU ni suma de clocks de diferentes APIs; timings.json lo declara.

Build y CPU self-tests PASS; provenance SHA256 de fuentes/EXE verificado.
Worker exit0, timeoutfalse, retry0. D3D12 debug errors0; warning820 de clear sin
optimized clear value preservado, rendimiento potencial menor pero clear válido.
Khronos validation no disponible, debug-utils errores observados0; no afirmar
validación Vulkan completa. Device lostfalse/removed reason0.

AMD/SR/Wisteria baseline hashes intactos. Sin Wisteria/runtime/PresentWorker,
Minecraft, DLSS-G, x3–x6, Dzn, traducción API ni cambios globales driver/perfiles.
Recursos y sincronización GPU nativos, Vulkan sigue API principal.
Siguiente fase futura: inputs A/B/depth/MV y output G1 D3D12-owned/importados
para el harness DLSS-G x2. **No ejecutada en esta fase.** Parada tras
documentación/commit/push autorizado. Evidencia completa local en
logs/runtime/dlssg-crossapi-interop/20261003-230013-062; Git sólo fuentes/texto.

## Histórico — primer probe de capabilities

Fase A se detuvo en **CAPABILITY**, run `20261003-223835-709`. La ruta literal
solicitada (memoria creada/exportada en Vulkan → OpenSharedHandle D3D12) no
superó el gate: los handles D3D12 consultados son importables en Vulkan pero
no tienen EXPORTABLE. No se probaron fases B–E ni otra variante automática.
Esto no demuestra inviabilidad de todo intercambio GPU entre las APIs.

Base `4c3efe98882e68e7b93039f1d868ea07655b7424`, main limpio. Módulo aislado
`experiments/dlssg-crossapi-interop`, compilado MSVC19.51 Release x64/MT.
No enlace NGX, carga comunitaria, DLSS-G, comandos GPU, swapchain, presentación,
Minecraft ni cambios de baseline. La prueba DLSS-G D3D12 x2 anterior sigue PASS;
el fallo Vulkan DLSS-G directo permanece cerrado y no se repitió.

## Identidad y configuración medida

Vulkan y DXGI seleccionaron exclusivamente NVIDIA RTX3050Ti Laptop GPU:
UUID01895b66d1ca454d88788dd21fdef638, LUID4c29010000000000, vendor0x10de.
SAME_GPU=PASS. D3D12CreateDevice12_0 tuvo éxito; device removed false/reason0.
Formato RGBA8 UNORM, VkImage2D/optimal, usage TRANSFER_SRC|TRANSFER_DST,
sin flags extra. Resolución prevista256x256; no se creó imagen Vulkan por
faltar el gate de exportación. D3D12 creó recurso compartido256x256/DEFAULT,
COMMON/flagsNONE y lo abrió por shared handle; fence compartido también abrió.
CreateSharedHandle/OpenSharedHandle success sólo prueba D3D12, no cross-API.

| Handle consultado | Resultado | Features | Importable | Exportable | Compatible | ExportFromImported |
|---|---|---|---|---|---|---|
| D3D12_RESOURCE (0x40) | VK_SUCCESS | 5=DEDICATED_ONLY+IMPORTABLE | Sí | No | 0x40 | 0x40 |
| D3D12_HEAP (0x20) | VK_SUCCESS | 4=IMPORTABLE | Sí | No | 0x20 | 0x20 |
| D3D12_FENCE (0x8) | consulta void | 2=IMPORTABLE | Sí | No | 0x8 | 0x8 |

VK_KHR_external_memory_win32 y external_semaphore_win32 están presentes.
Consulta de semáforo con tipo binario por defecto: timeline no consultado.
Los masks ExportFromImported se preservan; no reemplazan el bit EXPORTABLE
necesario para aceptar la ruta de exportación nativa solicitada.

[Khronos define las propiedades de memoria](https://docs.vulkan.org/refpages/latest/refpages/source/VkExternalMemoryProperties.html)
y [los tipos D3D12_RESOURCE/HEAP como handles de origen D3D12](https://docs.vulkan.org/refpages/latest/refpages/source/VkExternalMemoryHandleTypeFlagBits.html).
[Las propiedades de semáforo](https://docs.vulkan.org/refpages/latest/refpages/source/VkExternalSemaphoreProperties.html)
son una consulta separada. [Microsoft documenta compartir recursos/heaps/fences](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createsharedhandle).

## Límite y alternativa futura, sin ejecutar

CROSS_API_INTEROP=FAIL en el gate solicitado; shares/readbacks/sync/roundtrip
NOT_RUN. No hashes de píxeles ni generation PASS inventados.
DLSSG_VULKAN_SIDECAR_PREREQUISITES=NOT_READY.

Una alternativa pública candidata sería crear allocation/fence en D3D12,
exportarlos allí e importarlos en Vulkan. Vulkan podría producir contenido
en su VkImage asociado a esa allocation y D3D12 consumirlo después. El origen
de la memoria no determina la dirección de los píxeles. Import-only no
equivale a un único sentido de acceso GPU. La alternativa sigue SIN PROBAR:
no se implementó importación, escritura, ownership, layouts ni waits/signals.
Cambiaría el origen de allocations frente a la Fase B literal; se documenta
para revisión, sin empezar una variante tras el bloqueo.

La arquitectura sigue Vulkan, con D3D12 auxiliar. Esto no traduce command
streams, no utiliza Dzn ni convierte Minecraft a DirectX. Interop futuro
intercambiaría recursos y sincronización GPU. PresentWorker permanece intacto.

## Ownership, validación y tiempos

Handles NT creados por D3D12, propiedad aplicación. No duplicados ni importados
en Vulkan. OpenSharedHandle mantiene su referencia; aplicación CloseHandle
después de abrir cada recurso/fence. Sin handoff entre queues/API; estados
D3D12 COMMON y ningún layout/transición Vulkan ejecutado.

D3D12 debug layer activada: errores0. Khronos validation layer no disponible;
debug utils habilitado, errores Vulkan observados0. Esta ausencia limita la
validación y no se presenta como prueba completa de correctness del bridge.
Sin device lost y ningún dispatch GPU.

BRIDGE_CPU_COPY_COUNT=0 porque no hubo transporte. SAME_ALLOCATION_SHARED
cross-API NOT_DEMONSTRATED, GPU_COPY_REQUIRED NOT_DETERMINED; no se afirma
zero-copy funcional. CPU wall-clock del probe2076.974ms incluye initialization,
consulta y persistencia; no representa latencia de handoff ni roundtrip.
Timestamps GPU, handoffs y trabajo GPU NOT_RUN; no suma de clocks incompatibles.

El wrapper no capturó ExitCode nativo (null). Se conserva ese límite sin inferir
el código. Resultado JSON y eventos completos registran el gate; no hubo retry.
Para futuras ejecuciones el wrapper cachea el handle del proceso antes de
esperar. Ninguna nueva ejecución para corregir ese dato accesorio.

Evidencia completa `logs/runtime/dlssg-crossapi-interop/20261003-223835-709/`:
manifest/result/capabilities, reviewed-result, events, logs debug/validation,
resource-map/sync-map/hashes/timings. Originales preservados. Tests CPU del gate
y verificación SHA256 AMD/SR/Wisteria PASS. No modificación de esas baselines,
PresentWorker/ring/GL-Vulkan, driver/perfiles, Wisteria ni MFG>x2.
Fase detenida tras documentación y commit/push al repositorio del usuario.

## Integración Wisteria posterior — candidato, sin runtime PASS todavía

El contrato compartido se aplica ahora a un device Vulkan prestado por Wisteria,
con pool D3D12-owned persistente de ocho slots y dos submissions Vulkan separadas
por Evaluate D3D12. Imports/handles sólo en creación de pool. Inputs reales necesitan
cuatro GPU blits; G1 se leasea directamente. Runtime JNI y CPU tests compilan/pasan;
la prueba Minecraft aún no se ejecutó. CURRENT_REAL_PIPELINE.md y
WISTERIA_DLSSG_X2_INTEGRATION.md documentan la adaptación y sus límites.
Los PASS de este documento siguen siendo del harness aislado, no de Minecraft.

## Minecraft shadow integration — 2026-10-03, closed FAIL

Single run 20261004-010600-001: same GPU selected, JNI/public loader pass, but
D3D12CreateDevice returns0x887A0007 DXGI_ERROR_DEVICE_RESET. Persistent shared pool,
resource/fence imports and cross-API submits not reached. This does not invalidate
or repeat the closed standalone interop PASS; it prevents claiming Minecraft PASS.

Native embedded session reused standalone EnableDebugLayer during first Minecraft
job. Late enable is a plausible device-reset cause per Microsoft's public contract;
prior D3D12 device remains unproven. No debug-layer correction or retry applied.
Resource-pool/lifetime/resize behavior is implemented but runtime-unvalidated.
Per-frame GPU copies/waits are designed, executed transport copies0 in failed run.
Provider disabled cleanly; no D3D12 Present/new swapchain/provider vkQueuePresent.
See WISTERIA_DLSSG_X2_INTEGRATION.md and the run result for confidence/next step.
