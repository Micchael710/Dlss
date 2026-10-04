# DLSS-G MFG runtime validation

## Fase vigente — gating de output y preparación MFG, sin PASS gráfico nuevo

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

Native and Java21 bytecode builds PASS.21 SR CPU tests PASS, plus output-status contract counts1..4 PASS. Packaging verifies9 AMD classes and2 AMD DLLs unchanged; protected baseline hashes unchanged. JNI ABI2 linkage test PASS. No graphic gate passed by these build/CPU results.

The public event notification is documented by [Microsoft SetEventOnCompletion](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion); a non-null event is used, with a threadpool callback. Pool destruction drains callbacks outside the hot path.

## Único run x3 — FAIL, runtime no conforme

Run `20261004-042043-949`, implementación e92ad6b, seis Evaluate SUCCESS
(3 por índice), seis completions, count2; pool observado4 slots/24 images.
NGX reported max2, requested external max2; getter0x00000001 puede estar
intervenido por el loader. No es prueba de ejecución por sí solo.
64 kernel_create con nombres y status0; Evaluate backend real observado.

Flags tras GPU: interval1(reset) [1,1]; interval2 [0,-1]; interval3 [0,-1].
La readiness excepcional impidió aprobar el grupo y propagó crash Java
`Application-managed frame presenter failed / GPU disable metadata unavailable`.
El segundo flag no tiene valor válido. No equivaler -1 a disable0 ni a disable1.
No forzar el gate ni afirmar que el backend carezca de soporte MFG.
Auditoría de source: el buffer parte de0xffffffff; snapshot[0,-1] excluye
el catch-all de Map/fence, que rellena todos los índices con -1. Lo observado
es el valor inicial en index2; por qué no se actualiza sigue UNKNOWN.

No se alcanzó el umbral de muestras. Hashes y orden temporal UNKNOWN. La
traza Java quedó vacía porque se vaciaba sólo cada120 retirements y no hubo
cierre del writer después del crash. Las trazas nativas se conservaron. No
se pueden medir presentaciones, pacing o FPS desde esa traza vacía.
Dos completions cumplen Evaluate SUCCESS + disable0 + !reset + recurso
asignado; contenido validado0. Nunca presentar esto como dos Gi MFG probados.

La JVM del run fue Java21.0.12.1. El usuario corrigió que usa Java25; el launcher
ha sido actualizado a25.0.4, con auditoría del **javaLauncher de runClient**
sin ejecutar Minecraft. Bytecode mods21 conservado. La relación entre la
versión de Java y el fallo del flag no está demostrada. Java25 x3 NOT_RUN.

Stop después del fallo: x4/x5 NOT_RUN, x6 NO. Config aislada restaurada desde
snapshot si su hash todavía coincide con el preparado; nada en .minecraft real.
Resultado cerrado en `result.json`, manifiesto cerrado derivado en
`run-manifest.json`; `manifest.json` preparado original conservado.
Reviewer offline `tools/review_minecraft_dlssg_mfg_failure.py` sólo cierra
este fallo observado y nunca declara PASS ni ejecuta GPU.
