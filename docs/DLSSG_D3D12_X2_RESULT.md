# DLSS-G SM86 D3D12 offscreen x2 — 2026-10-03

**DLSSG_SM86_D3D12_X2=PASS** en la única ejecución autorizada después del fix
del recorder: `20261003-222208-087`. Es generación de una fixture sintética
offscreen en esta máquina; integración Minecraft, gameplay, presentación y
rendimiento sostenido todavía no demostrados.

Base `aacc6d26d5eb85e363d7bd4c946d2cda641dbbc0`, main. RTX3050Ti Laptop GPU,
vendor0x10de/device0x25a0, LUID4c29010000000000, D3D12CreateDevice con feature
level12_0 exitoso, driver596.49. sdli0.3.5/runtime NVIDIA310.9.1 identificados
por SHA256 en manifest. LoadLibrary/INI público, DLL viva durante el worker.
Sin OptiScaler ni Streamline cargados; sin ABI privada invocada por el host.

## Validación

Loader LOADED; backend OBSERVED_ACTIVE; NGXInit PASS. Getters success0x00000001:
Available1, FeatureInitResult1, Max1, NeedsUpdatedDriver0. Provenance
REPORTED_POTENTIALLY_HOOKED; capabilities no constituyen prueba de generación.

Fixture A/B/depth/MV/sentinel roundtrip exacto PASS. A/B/G1 recursos distintos.
AllocateParameters/CreateFeature0x00000001, handle0000023117D35D18.
Cuatro warmups frames0..3, reset sólo frame0. Target frame4: B actual,
historia A, count1/index1, resetfalse. Evaluate0x00000001; submit/completion
fence39 PASS. Consulta pública final GetDeviceRemovedReason0x00000000,
device removedfalse; worker exit0, no timeout. OutputDisableInterpolation0.
Inputs A/B permanecen intactos. Generated_count_confirmed1.

G1 difiere en42424 píxeles de A y42757 de B, no coincide con sentinel.
Centro objeto x451.408675 entre A443.5/B459.5, y359.481273; fondo con cero
píxeles inválidos. Error RGB medio frente a referencia intermedia0.298147/255.
VerdictReducer temporal PASS. Inspección breve PNG confirma escena completa y
objeto intermedio, sin negro/corrupción grave. La fixture no contiene HUD real.

| Recurso | SHA256 canónico RGBA |
|---|---|
| A | 9285dff3162262072b299c4cbe94b40e9e51280c27ef9f7e38a08abe730cc140 |
| B | 4acff001b7f04ce1ce503314fbdd0dfadc6f2cf87d843dcf12365319bf387baa |
| Sentinel | 80ebb6cf65541911010912a1a2d2d35cb683f935d1f0b15dc8748f84d473c586 |
| G1 | b5261cc3c32720a2e4178611c9cb25d5cb964e4b554eeb6b449ec311f7993a3e |

## Kernels y preservación

result.json original decía kernel FAIL_OBSERVED al mezclar seis diagnósticos
sin imagen/entry/shader (status privado-14) con kernels reales. Original
conservado, reviewed-result.json separa esos eventos; sin interpretar-14.
ID local backend1 y enum público NGX FrameGeneration11 son namespaces distintos.

Backend backend_1402248.jsonl línea16: Kernel_BlendCandidatesFused,
image=cubin_sm86, bytes17056, routedtrue, shader válido, status0. Línea17
cu_function_create status0; línea204 cu_chain_launch de ese kernel status0.
Evaluate líneas210..213: count1/index1, ningún launch fallido.
D3D12_KERNEL_CREATE=PASS_OBSERVED para kernels reales; referencias completas
en reviewed-result, diagnósticos conservados en diagnostic_kernel_evidence.

D3D12 validation_errors0. Dos mensajes informativos1328 repetidos por volcado
final indican buffers creados efectivamente COMMON; no son errores/corrupción.
Recorder CPU self-test PASS incluyendo lock/recovery. Preflight sólo añadió
consulta pública final device removal y gates loader/backend obligatorios al
reducer. No rediseño recorder ni cambio generación. Clasificador corregido
después mediante análisis CPU/regression test; no nueva ejecución GPU.
CPU Evaluate target1.352100ms; tiempo GPU sin instrumentar. Persistencia/logging
y readbacks contaminan tiempos de eventos: no benchmark ni medición FPS.

## Siguiente arquitectura, sólo documentada

DIRECT_VULKAN_X2=FAIL_AT_KERNEL_CREATION histórico; VULKAN_RETESTED=NO.
D3D12_OFFSCREEN_X2=PASS; SIDECAR_ARCHITECTURE_JUSTIFIED=YES para diseño futuro:

Minecraft/OpenGL → captura existente SR/Wisteria → recursos Vulkan → memoria
GPU compartida Win32 → recursos D3D12 → DLSS-G → output GPU compartido →
VkImage → PresentWorker Vulkan.

Intercambio de recursos/sincronización, sin traducción completa de APIs.
Vulkan sigue principal; PresentWorker único dueño de presentación. Investigar
recursos/heaps D3D12 compartidos/external memory Vulkan y fences D3D12/
semáforos externos Win32 sobre la misma RTX. Formatos, handles y ownership
pendientes de demostrar. No CPU copy, GPU→RAM→GPU, Dzn ni segundo presenter.
Bridge todavía no implementado.

Medir separadamente captura OpenGL/Vulkan, handoff Vulkan→D3D12, ejecución
DLSS-G, handoff D3D12→Vulkan, cola PresentWorker, real/presented FPS, pacing,
latencia añadida extremo a extremo y VRAM. Objetivo zero-copy/near-zero-copy
GPU, sin promesa de latencia cero.

AMD/SR/Wisteria conservan hashes aprobados (validation-evidence.json).
No Minecraft, cambios driver/perfiles/DRS, sidecar ni x3–x6. Parada aquí.
Evidencia completa local logs/runtime/dlssg-external-harness/20261003-222208-087/.
Git sólo fuentes/scripts/documentación/evidencia textual; imágenes/readbacks/
binarios locales. Redistribución de runtimes/backend pendiente antes de empaquetar.
