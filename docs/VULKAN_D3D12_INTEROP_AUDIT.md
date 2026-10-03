# Vulkan/D3D12 — probe de capabilities, 2026-10-03

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
