# Revisión Vulkan x2 y ensayo D3D12 offscreen

2026-10-03. Base local/GitHub `65528d3528a3ba97d8fa94b0b1b1d5557e13deb4`,
main, repositorio del usuario https://github.com/Micchael710/Dlss.git.
Se continúa el harness original. No reset/rebase ni restauración destructiva.
El cambio local inicial de .gitignore era una línea aislada `0`; se reemplazó
por exclusiones explícitas de caches/checkpoints y wrapper JAR no versionable.
El JAR local permanece en disco.

## Run Vulkan preservado: 20261003-205509-501

La revisión escribe únicamente reviewed-result.json y reviewed-summary.txt.
Resultado, manifest, eventos, callback y logs originales conservan sus hashes,
registrados en el resultado derivado.

| Gate | Observación |
|---|---|
| Build / loader | PASS / LOADED, sdli0.3.5 identificado por SHA256. |
| Backend | OBSERVED ACTIVE: backend_install.status0, install.active=true. |
| NGX Init | PASS, 0x00000001. |
| Architecture | PASS adaptado: callback L305/L327, física0x170 y mínimo0x170. |
| Fixture roundtrip | PASS: A/B/depth/MV/sentinel iguales a CPU. |
| CreateFeature | FAIL, 0xBAD00002 = NVSDK_NGX_Result_FAIL_PlatformError. |
| Kernel creation | FAIL_OBSERVED: callback L479. |
| Evaluate / completion del target / G1 | NOT_RUN; fences previos corresponden a fixture. |
| Generated count | 0. |

El callback L479 registra:

```text
NGXCubinVulkan::CreateKernel:498
Error: vkCreateCuModuleNVX() for Kernel_BlendCandidatesFused failed -3
```

L481 registra Failed to create DLSSG instance. Se corrige el resumen anterior
NOT_OBSERVED: el intento y el fallo del kernel sí fueron observados. El callback
público del runtime es evidencia suficiente de esa etapa, sin ABI privada.

`-3` es `VK_ERROR_INITIALIZATION_FAILED`, según vulkan_core.h L149 y la
[definición pública VkResult](https://docs.vulkan.org/refpages/latest/refpages/source/VkResult.html).
La [referencia vkCreateCuModuleNVX](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateCuModuleNVX.html)
declara ese tipo de retorno y fallo. No publica especificación completa del
contenido del módulo. El código identifica inicialización fallida por razones
de la implementación; no demuestra incompatibilidad de cubin, corrupción,
falta de memoria ni un defecto concreto del driver/backend.

IF_FAIL_STAGE=kernel creation. Confianza alta en etapa/API/código.
VULKAN_ROOT_CAUSE=UNKNOWN para la razón interna del rechazo.
El backend anuncia actual_sm86, target_sm86, routerSM86, image=cubin_sm86,
runtime_profile310.9.1.0. Esto acredita instalación/configuración, no creación
de kernels compatibles en Vulkan. No se reconstruye su funcionamiento privado.

## Revisión pública del host Vulkan

| Elemento | Contrato y evidencia |
|---|---|
| GPU | NVIDIA3050Ti, UUID01895b66d1ca454d88788dd21fdef638, LUID4c29010000000000; iGPU rechazada. |
| Device/queue | vkCreateDevice=0; family0 graphics+compute; queue y primary command buffer registrados. |
| Extensions | Queries NGX públicas Success; sólo se habilitan nombres anunciados, se exige toda extensión device requerida. |
| Enabled | VK_NVX_binary_import, VK_NVX_image_view_handle, KHR_buffer_device_address, push_descriptor, external_memory[_win32], external_semaphore[_win32]. |
| Features | Subconjunto Vulkan1.2/flags shader sólo si soportados; ningún feature NVX inventado. |
| NVX function | Callback registra un retorno de vkCreateCuModuleNVX: no es puntero ausente. Extensión habilitada y device con queue, como exige el contrato público. |
| Create params | Helper oficial nvsdk_ngx_helpers_dlssg_vk.h L47–63, Width/Height1280x720, RGBA8_UNORM VkFormat37, node masks1/1. |
| RenderWidth/Height | Host los llena; helper Vulkan fijado no los transmite. No se inventa clave alternativa ni se atribuye a eso el fallo CuModule. |
| Resources | VkImages A/B/G distintos, image views, optimal tiling, sampled/storage/transfer; depthR32F y MVRG32F; formatos soportados comprobados. |
| Layout/sync/lifetime | GENERAL con barreras; diez submits fixture y fences completados. Recursos vivos en Create; Create falla antes de submit de creación. DLL no se descarga. |
| Validation | Errores registrados0; Khronos validation layer ausente. No es validación completa del runtime. |

Warning Width/Height deprecados (L475) corresponde al helper oficial fijado.
nvsdk_ngx_defs_dlssg.h permite las claves específicas como reemplazo con
precedencia si se establecen; runtime acepta legacy y llega a CreateKernel.
No es un error concreto de parámetros demostrado que explique CuModule=-3.
La variante D3D12 nueva usa las claves públicas de reemplazo; Vulkan histórico
y capabilities permanecen sin modificación. RTSS/OBS/NVIDIA overlay aparecen
en módulos del run: se conserva esa limitación del entorno sin atribuirles el
fallo ni modificar aplicaciones/perfiles globales.

No se encontró un error público del host que explique el rechazo.
SECOND_VULKAN_RUN_PERFORMED=NO.
DIRECT_VULKAN_X2=FAIL_AT_KERNEL_CREATION sólo para RTX3050Ti Laptop,
driver596.49, runtime310.9.1, sdli0.3.5 y entorno de este run.
No generalizar a otras GPUs/versions/drivers/adaptaciones.
NEED_D3D12_SIDECAR=YES_FOR_EVALUATION sólo justifica el ensayo D3D12,
sin demostrar que un transporte sidecar sea necesario o suficiente.

## Una ejecución D3D12 offscreen: 20261003-214903-641

Mismo ejecutable, worker separado, mismo componente/runtime; NVIDIA LUID exacto,
feature level12_0, debug layer disponible. Sin swapchain/Present/Minecraft,
Wisteria, interop híbrido ni MFG. Loader/backend activos, NGX Init y capability
query Success. Host guardó Available1 y NeedsUpdatedDriver0.

Worker exit3221226505 / 0xC0000409, sin timeout. No llegó a fixture,
AllocateParameters, CreateFeature ni Evaluate. worker-result.tmp conserva
**Evidence checkpoint failed** durante la escritura del getter max. Su getter
devolvió Success, pero el valor no quedó guardado; no se sustituye por el valor
de una traza secundaria del backend.

Recorder original lanzaba si MoveFileExW fallaba; el catch volvía a guardar el
error usando la misma operación fallida. Se observó un problema del host al
persistir evidencia. No se capturó el Win32 code original ni stack de salida:
la doble excepción explica una ruta posible de terminación, sin atribuir una
causa interna del runtime. [Microsoft documenta 0xC0000409 como fast-fail](https://learn.microsoft.com/en-us/cpp/intrinsics/fastfail?view=msvc-170);
sin su parámetro adicional no se concluye stack overflow. Consulta acotada
Application Error no recuperó un evento del harness.

Backend registra tres kernel_create diagnósticos: bytes0, entry vacía,
routed=false, feature1 y status-14 durante capability discovery. Se conservan;
no son creación de nuestra feature FrameGeneration11. No se deduce el significado
privado de-14. **D3D12_KERNEL_CREATE=NOT_RUN** para la feature solicitada.

IF_FAIL_STAGE=PRECONDITION / EVIDENCE_PERSISTENCE_DURING_CAPABILITY_QUERY.
Confianza alta en error de host observado; Win32/fast-fail subcode originales
UNKNOWN. D3D12 NGX_INIT=PASS; gates de generación NOT_RUN, generated0.
DLSSG_SM86_D3D12_X2=FAIL de run incompleto; D3D12 x2 sigue sin demostrar,
no se declara inviable. Device removed final=null porque no hubo consulta final;
no hay evidencia de device lost/reset. Resultado y logs originales preservados,
interpretación nueva en reviewed-result.json.

No se repite D3D12. Se corrige recorder para retries CPU acotados de reemplazo
por sharing/access, code Win32, recovery JSON y manejo sin doble throw de IO.
Un error de persistencia aborta antes de GPU fixture/feature. Compilado y
probado sólo en CPU, con checkpoint deliberadamente bloqueado. No otra prueba GPU.

## Validación y parada

Harness build=PASS. CPU veredicto rechaza duplicados/sentinel/black/píxel aislado/
corrupción, acepta fase intermedia y comprueba matrices/depth. Test lock/recovery
PASS; clasificadores de callback PASS. Capabilities nunca producen PASS FG.

AMD/SR/Wisteria/bridge mantienen SHA256 aprobados; sin cambios de mods,
PresentWorker/ring/interop/runtime-baseline. Minecraft no lanzado.
No ABI privada, parche NVIDIA, extracción de kernels, perfiles/DRS ni binarios
en commit. SIDECAR_ARCHITECTURE_JUSTIFIED=NO: falta D3D12 x2 real.
Siguiente fase requiere autorizar nueva prueba D3D12 tras corrección de evidencia;
no transporte ni x3–x6. Esta fase se detiene conservando ambos runs.

## Continuación autorizada — D3D12 offscreen x2 PASS, run20261003-222208-087

Una única ejecución posterior al fix, sin repetir Vulkan. Loader/backend activos,
NGXInit/fixture/Create/Evaluate/completion/output temporal PASS. G1 != A/B/
sentinel, generated1; device removedfalse/0 y cero errores D3D12. Kernels SM86
reales PASS_OBSERVED, incluyendo Kernel_BlendCandidatesFused. Seis diagnósticos
vacíos conservados aparte; ID backend1 no equivale al enum público NGX11.
Reducer original mezclaba contextos: result.json intacto, reviewed-result nuevo.
DIRECT_VULKAN_X2=FAIL_AT_KERNEL_CREATION permanece. D3D12_OFFSCREEN_X2=PASS.
SIDECAR_ARCHITECTURE_JUSTIFIED=YES para diseño futuro de memoria/fences GPU
compartidos; sin traducción completa ni CPU roundtrip. Bridge no implementado,
sin Minecraft/MFG>x2. Baseline AMD intacta. Parada aquí.
Detalles [DLSSG_D3D12_X2_RESULT.md](DLSSG_D3D12_X2_RESULT.md).
