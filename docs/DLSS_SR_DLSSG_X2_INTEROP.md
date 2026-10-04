# DLSS SR -> DLSS-G x2: contrato experimental y fallo observado

## 2026-10-04 — second-submit diagnostic, combined outcome FAIL

One combined run: `20261004-201953-044-handoff-x2`. No standalone SR rerun, AMD run or x3+ run. Start HEAD `d9648f281d9b9316d9efa615da231b0cd1a864da`.

The first three native input submissions returned VK_SUCCESS and each reached D3D12 completion and slot retirement. The previous second-submit VK_ERROR_DEVICE_LOST did not recur. Observed FG: Create pools 3, Evaluate 3721, completion callbacks 3721, valid intervals 3661, generated presents 3660; REAL → G1 → REAL observed True. These are public worker present events, not a scanout capture.

**Combined FAIL:** the first OpenGL error was `GL_INVALID_OPERATION: Wait for sync object failed` in `glWaitSemaphoreEXT`, from `FrameResourcesSet.awaitCaptureRelease`, launcher line 1174. The user closed the game normally, but the process exited -1073740791 (`0xC0000409`); Gradle exit 1. Native bridge recorded DRAINED, device removed false. SR shutdown completion was not observed. No retry. Original device-loss root cause remains **UNKNOWN**; this run changed readiness isolation and source barrier scopes together, so it cannot attribute the previous fault to one change.

Implementation: persistent borrowed readiness pair per capture slot, one signal/consume cycle retired after the existing output fence; no new per-frame host waits or CPU frame transport. Reset/retirement guards remain. JNI ABI4 adds device-fault/checkpoint configuration and first-three-interval state. Diagnostics are opt-in. Optional device_fault feature is queried and enabled correctly; vendor binary is disabled. Both fault and checkpoint extensions were enabled, but neither fault data nor queue checkpoint snapshots were queried because VK_ERROR_DEVICE_LOST did not occur. Checkpoint markers remain alive until slot retirement.

Preflight: actual NVIDIA Vulkan format/usage/extent/sample queries PASS for R32F and RG16F → RG32F floating-point color NEAREST blit. COLOR aspect, mip0, layer0/count1, samples1, 495x278; linear filtering is unused. No compute conversion or CPU conversion. Vulkan validation layer is absent; zero captured VUIDs is **not** a validation PASS. CPU tests rejected 13 stale/readiness cases, 8 binary/pending reuse cases and 4 native slot/command cases. Actual JNI load ABI4, C++JNI, SR NeoForge and Wisteria builds passed. AMD 9 classes + 2 DLLs remain byte-identical by hashes; historical MFG x3 evidence retained.

NGX SDK section 3.4 guarantees input resources return to read state. Caller declares SHADER_READ_ONLY_OPTIMAL; borrowed source barriers remain 5 → 6 → 5, now with explicit compute sampled-read ↔ blit transfer-read scopes, using enabled sync2 in the diagnostic route. This is the documented/caller contract, not a measured image-layout query. [Pinned NVIDIA SDK guide](https://raw.githubusercontent.com/NVIDIA/DLSS/374959484e79a640feaba44c93ac8cfb0a03f5b5/doc/DLSS_Programming_Guide_Release.pdf), [Vulkan blit contract](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html).

Config restored byte-for-byte: `ade8502bc8d5d523397b5e1838e1f9100d9782ff116c12ef7484594095172462`. Baseline/live launcher unaffected. Exported evidence: `logs/runtime/dlss-sr/20261004-201953-044-handoff-x2/fg-two-interval-state.jsonl`, prior two-interval comparison, format preflight, build logs, bounded runtime samples, first GL error stack, shutdown result and runtime-result.json. Full existing raw telemetry and binary samples remain local; no binary upload. Presentation owner remains PresentWorker.

Current blocker: **GL_RELEASE_WAIT_FAILED_AND_NATIVE_SHUTDOWN_CRASH**. Do not call this validated SR+x2. Native crash cause and original lost cause UNKNOWN.


La única ejecución de esta iteración, desde b3dd23c, es
`logs/runtime/dlss-sr/20261004-181754-546-handoff-x2/`. Resultado FAIL.
Java25.0.4, bytecode21, MC1.21.1/Neo21.1.219. No standalone repetido.

## Productor y join implementado

GL preprocess escribe depth R32F y motion RG16F compartidos a render495x278;
GL glFinish declara SHADER_READ_ONLY y el submission SR main lo espera.
Antes, captureReady sólo se señalaba desde GL antes del uso SR Vulkan: no
ordenaba el último lector. Ahora el mismo submission que contiene NGX SR
señala upscaleFinish y los dos captureReady al terminar. Receipt inmutable
publicado sólo tras submit exitoso: frame/generación capture, device, queue,
family/index, command/generation/fence, VkImages/formats/extents/layouts y
ready semaphores. No declara completion CPU por publicar el receipt.

El guard borrowed exige ese receipt fresco y los mismos bindings. Se rechaza
device/family diferente (no se implementa ownership transfer entre families),
identidad queue inconsistente, señal ausente, fuente inexistente/layout cambiado
o segundo claim. El native ABI3 valida device y producer queue real mediante
vkGetDeviceQueue y exige ambos semáforos en el input submit. Los formatos y
extents se validan realmente, incluyendo soporte BLIT_SRC/BLIT_DST del device.

## Consumer existente, sin segundo interop

| Recurso | Source | Destino persistente FG | Extent |
|---|---|---|---|
| Depth | VkImage2952963527680, R32F/Vk100 | Slot0 Vk2956805655520, R32F/Vk100 |495x278|
| Motion | VkImage2952963538096, RG16F/Vk83 | Slot0 Vk2956805643616, RG32F/Vk103 |495x278|
| Final/HUDless | Owned capture | RGBA8/Vk37 D3D12 shared |854x480|

Producer queue2953834132176, family0/index0; consumer2953834146192,
family0/index1. Los semáforos depth2956992840864/motion2956992839040 son
binary: no valor timeline de readiness source. Primer frame lógico2616,
capture generation2749, realFrameId1, producer command2952971830960,
submission generation1/fence2957204802624. Segundo lógico2617,
capture generation2750/realFrameId2, command2952971817104,
generation1/fence2956995643440. Las fuentes siguen perteneciendo a SR.

Native input command2956763908064 espera las señales del primer productor;
blit depth/motion registra source SHADER_READ_ONLY(5)->TRANSFER_SRC(6)->5.
Destino nuevo UNDEFINED->GENERAL; pool5 slots/25 images se crea una vez.
RG16F->RG32F usa vkCmdBlitImage, no staging/copia CPU. El source no se destruye
ni se entrega al D3D12 directamente. Cada slot destino tiene memoria propia.
La señal shared timeline ready1 ordena D3D12.queue.Wait, Evaluate y done2.
El output Vulkan espera done2 y las señales release ordenan la siguiente
escritura GL. GL server wait evita rendezvous CPU con el worker independiente;
los rings conservan su backpressure Vulkan de reutilización. Nada de device/
queue idle, WaitForSingleObject ni polling en el join por frame.

Estos son comandos/contratos observados, NO validación completa de layouts:
no se capturó el estado efectivo post-NGX. El device lost impide afirmar
que esta implementación preserva correctamente todos los estados en GPU.

## Gates y stop

Antes de mundo: Vulkan/NGX SR Init PASS, capability Available1, tres SR
Create válidos (reloads de recursos), session FG capability count1. El
contrato estaba compilado/configurado; el join GPU sólo se observa después
de entrar al mundo. No se fabrica un PASS GPU pre-world.

CPU13 casos rechazados: missing receipt/source, wrong frame/generation,
queue/fence/device/family mismatch, source lifetime/binding, missing wait,
destination alias y layout sin restaurar. JNI ABI3 se cargó realmente sin
NGX/GPU. C++/SR/Wisteria build PASS. No fixture GPU adicional: harness previo
no cubre el productor GL/SR. AMD9 clases/2DLL byte-identical, sin rerun.

FG Create1 y Evaluate2 result1, count1/index1. Primer input submit exitoso,
callback done2/status1 RESET, G1 descartado por reset, un REAL presentado.
Eso prueba indirectamente completion del primer input copy vía shared fence;
no hay evento de retirement completion, porque no pudo retirarse la lease.
Segundo input vkQueueSubmit devuelve -4/VK_ERROR_DEVICE_LOST. GPU_SUBMISSION
es el primer fallo de adaptador en launcher.log:1421. El native no publicó
un segundo submit exitoso. Causa subyacente GPU/ordering/layout UNKNOWN.
No atribuir el device lost al driver, matriz o formato sin evidencia nueva.

La proyección del frame2615 incluye Infinity en m00/m11 y el guard se conserva;
la matriz completa está en launcher.log y runtime-result.json. Los dos frames
siguientes construyen metadata y llegan a Evaluate: no fue el gate primario.

SR Evaluate4/outputQueued4/completionEvents0. G1 present0. No validación visual
completa por fallo temprano. PresentWorker sigue dueño único, sin añadir
Present D3D12/Vulkan al provider; no hay API trace de esas llamadas en este run.
STOP sin segundo intento. Crash Java: capture slot unrecoverable no se reutiliza,
cliente exit-1/0xffffffff y Gradle1. Shutdown normal NO; sin 0xC0000409 observado.
No se fuerza liberar pools que el native retiene como unsafe tras device lost.

Config restaurado exactamente:
`ade8502bc8d5d523397b5e1838e1f9100d9782ff116c12ef7484594095172462`.
MFG x3 histórico intacto: G2 bounded generation PASS, optional status UNKNOWN
0xffffffff, Minecraft x3 BLOCKED_STATUS_SEMANTICS_UNPROVEN. Ningún x3/x4/x5/x6.
