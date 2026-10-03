# DLSS-G en RTX 3050 Ti: investigación, 2026-10-03

Actualización vigente: [DLSSG_MFG_AUDIT.md](DLSSG_MFG_AUDIT.md). Implementación/runtime
detenidos por el usuario. La profundización posterior guardó metadata PE/hash de un
proxy en evidencia aislada, sin cargarlo; esta auditoría inicial era de textos.
La fase MFG no descargó ni ejecutó DLLs. API MFG confirmada; máximo local y backend
SM86/Vulkan siguen pendientes. DLSS SR no es el objetivo de la fase actual.

## Decisión

**D — UNKNOWN / MORE RESEARCH NEEDED**, específicamente para DLSS-G adaptado
SM86 → Vulkan → VkImage → nuestro PresentWorker. No es una afirmación de
imposibilidad física. Tampoco hay evidencia suficiente para declarar A o B.

DLSS Super Resolution está oficialmente soportado en RTX 30; DLSS Frame
Generation oficial no lo está. La [tabla NVIDIA](https://www.nvidia.com/en-eu/geforce/technologies/dlss/)
distingue ambos. La adaptación comunitaria merece investigación, pero el soporte
oficial Vulkan para otras GPU no demuestra que la adaptación funcione en esta.

No se implementó, activó ni empaquetó `wisteria:dlssg_ampere`. No hubo ejecución
de Minecraft, compilaciones, DLL drop-in, cambios de perfil NVIDIA, push o PR.
AMD_FSR_FG_X2 sigue siendo PASS conservado, sin repetir sus gates.

## Evidencia reproducible

Se guardaron árboles completos no truncados, textos seleccionados y SHA256 por
archivo en `logs/research/dlssg-ampere/<propietario>__<repo>/manifest.json`.
`tools/audit_dlssg_sources.py` descarga exclusivamente textos; no descargó DLL,
ZIP de runtime, modelos ni cubins. `local-evidence.json` verifica 197 textos.

| Repositorio | Commit auditado |
|---|---|
| sdli1995/dlssg_for_sm86 | 9621db573e07ed54f50c15bbb585ed9a7bdfac28 |
| NVIDIA/DLSS | 374959484e79a640feaba44c93ac8cfb0a03f5b5 |
| NVIDIA-RTX/Streamline | 2122257e0fce486f91b385aa63b9a09b0a34b363 |
| Nukem9/dlssg-to-fsr3 | 0e253f63c3c66ee7582406bfa3dec698df7ab2c1 |
| optiscaler/OptiScaler | 6b91154867affa0b6874bc1487b1cf0c8e257060 |
| pipotoufikxyz-lgtm/dlssg_for_sm86-MFG-version | cd4b4bb9f483ee3c59d3a912cd4075d1c8ebc742 |
| thierbig/bg3fgvk | 66082e6f691fba1c48e319b688a25fa5ae196e56 |

## Qué hace dlssg_for_sm86 y qué no podemos auditar

Según su [README fijado](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/README.en.md),
0.3.5 adapta el runtime NVIDIA; no sustituye FG por AMD FSR. 0.3.0 abandonó el
host NGX propio y volvió a un proxy sobre el runtime original. Publica resultados
en otras RTX; no hemos reproducido sus resultados en la 3050 Ti. Corrigió defectos
de recreación de feature y de spoofing que podían provocar corrupción o reset
del driver. Es una ruta experimental, no un backend NVIDIA certificado.

Su [guía de instalación](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/docs/INSTALL.en.md)
describe estas capas:

| Capa | Mecanismo documentado |
|---|---|
| Loader/proxy | Intercepta LoadLibrary; extrae runtime/backend emparejados por hash; no es el algoritmo |
| Gate juego/Streamline | NvAPI_GPU_GetArchInfo devuelve otra arquitectura únicamente a determinados consumidores |
| Gate NGX core/runtime | El mínimo exportado por NVSDK_NGX_GetGPUArchitecture debe concordar con hardware físico; no basta engañar al juego |
| Backend | Instala sustituciones de kernels; existe negociación DlssgMod_GetInfo/Install y SupportedRouteFlags |
| Cómputo | PTX/cubins SM86 extraídos/recompilados; variantes de inferencia e imágenes; dependencia CUDA/NVAPI |
| D3D12 | Incluye llamadas NvAPI_D3D12_LaunchCuKernelChain y recursos/comandos de esa API |

Incluso el modo stock numérico necesita kernels compatibles: 310.9 incluye
imágenes sm_89 incompatibles con Ampere. Las optimizaciones no equivalen a un
simple cambio de la disponibilidad en la GUI.

**Límite comprobado:** el árbol actual del repo principal contiene binarios y
documentación, **cero `.cpp/.c/.h/.hpp`**, sin build reproducible ni LICENSE completo.
Las referencias a `src/fg_gate.cpp`, `src/spoof_policy.h`, `docs/ARCHITECTURE.md`,
capturas y manifests de kernels no están en ese árbol. No pudimos trazar código
CPU→kernel, identificar todas las firmas/RVA, ni comprobar qué hooks se podrían
separar para Vulkan. La [solicitud de fuentes #524](https://github.com/sdli1995/dlssg_for_sm86/issues/524)
corrobora esta limitación, pero nuestro dato principal es el árbol descargado.
No se desensambló ni ejecutó su proxy.

## Vulkan: API real frente a adaptación pendiente

El [header oficial DLSS-G Vulkan](https://github.com/NVIDIA/DLSS/blob/374959484e79a640feaba44c93ac8cfb0a03f5b5/include/nvsdk_ngx_helpers_dlssg_vk.h)
incluye `NGX_VK_CREATE_DLSSG`, `NGX_VK_EVALUATE_DLSSG`, un command buffer y
`pOutputInterpFrame` como recurso Vulkan proporcionado por la aplicación. El helper
llama a CreateFeature/EvaluateFeature; no adquiere ni presenta el swapchain.
Esto permite diseñar un backend directo manteniendo nuestra presentación, pero
no elimina la comprobación de GPU ni aporta los kernels SM86.

Streamline sí contempla Vulkan. Su [guía DLSS-G](https://github.com/NVIDIA-RTX/Streamline/blob/2122257e0fce486f91b385aa63b9a09b0a34b363/docs/ProgrammingGuideDLSS_G.md)
describe inicialización temprana, slSetVulkanInfo, eVulkanSupported, Reflex,
HAGS y óptical flow Vulkan/interop. Su integración habitual intercepta
vkAcquireNextImageKHR/vkQueuePresentKHR y coordina generación/pacing. Introducirla
directamente competiría con el ownership solicitado de PresentWorker.
El plugin sl.dlss_g es cerrado según el [README](https://github.com/NVIDIA-RTX/Streamline/blob/2122257e0fce486f91b385aa63b9a09b0a34b363/README.md).
Streamline no es una dependencia demostrada del helper NGX directo; sí lo es de
la integración de alto nivel que publica NVIDIA. No copiar sus hooks de present.

### Parche comunitario encontrado y auditado

[pipotoufikxyz-lgtm](https://github.com/pipotoufikxyz-lgtm/dlssg_for_sm86-MFG-version)
anuncia Vulkan, pero su [estado explícito](https://github.com/pipotoufikxyz-lgtm/dlssg_for_sm86-MFG-version/blob/cd4b4bb9f483ee3c59d3a912cd4075d1c8ebc742/VULKAN_SUPPORT.md)
admite que el paquete no lo implementa de extremo a extremo.
`src/vulkan_route.cpp:110–115` graba y envía comandos, pero el algoritmo se delega
a `FeatureAdapter::Evaluate`. `vulkan_ngx_loader.cpp:17–30,58–63` resuelve nombres
de exports; no llama a una implementación NGX adaptada con firmas reales.
Faltan adapter NVIDIA y adaptación SM86 Vulkan. Su descripción comercial no es
un gate PASS. La ABI/genérico renderer pueden servir de referencia, no de FG.

[bg3fgvk](https://github.com/thierbig/bg3fgvk) usa Streamline con Vulkan en otro
juego. `src/vkhooks.cpp:253–268` conecta generación al present del interposer;
`src/slboot.cpp` inicializa plugins. No publica adaptación SM86 ni un resultado
3050 Ti; copiarlo no cumple nuestro contrato de presentación independiente.

### Nukem y OptiScaler: componentes reutilizables, con límites

En [Nukem NvNGXVulkan.cpp](https://github.com/Nukem9/dlssg-to-fsr3/blob/0e253f63c3c66ee7582406bfa3dec698df7ab2c1/source/maindll/NGX/NvNGXVulkan.cpp),
CreateFeature instancia FFFrameInterpolatorVK y Evaluate llama a su Dispatch.
`FFFrameInterpolatorVK.cpp` usa FidelityFX Vulkan. Puede enseñar mapeo de
parámetros/resources/layouts; **su algoritmo es AMD FSR**, no DLSS-G NVIDIA.

En [OptiScaler IFeature_VkwDx12.cpp](https://github.com/optiscaler/OptiScaler/blob/6b91154867affa0b6874bc1487b1cf0c8e257060/OptiScaler/upscalers/IFeature_VkwDx12.cpp)
hay recursos compartidos Win32, D3D12Resource, VkImage y fences/semaphores.
Eso prueba que interop VK/D3D12 es una técnica implementada, no que su DLSS-G
funcione en SM86. `framegen/dlssg/DLSSG_Dx12.h` se apoya en Streamline/DXGI
y creación/present del swapchain; VkOn12 observado corresponde a upscaling.
OptiScaler sigue OFF y no es nuestra ruta.

### Bloqueos técnicos concretos

1. No hay fuente ni ABI suficiente publicada del backend SM86 principal para
   conectar su sustitución de kernels al runtime **Vulkan**.
2. Las variantes PTX/cubin no se convierten en shaders SPIR-V simplemente
   cambiando un VkImage. Hay que demostrar su ruta de ejecución compatible,
   incluyendo optical flow, CUDA/NVAPI, recursos y sincronización.
3. Deben pasar requisitos del core NGX, runtime y feature, no sólo el selector.
   HAGS, driver y fallo exacto deben consultarse, no suponerse por el modelo GPU.
4. Hay que conservar device/queue/lifetime y separar la presentación de SL.
5. Una vía D3D12 auxiliar sería **hipótesis B**, no B demostrada: requeriría
   device por LUID coincidente, imágenes exportables compatibles (no asumir que
   las actuales memorias GL/VK aceptan también D3D12), copias cuando sea necesario,
   fence D3D12 ↔ semaphore Vulkan y evaluate offscreen sin DXGI present.
   El acceso al backend SM86 independiente sigue sin demostrar.

## Inputs del proyecto frente a DLSS-G

Inspección del código actual, sin nueva validación runtime. La API NVIDIA exige
coherencia espacial y temporal; el PASS AMD no prueba calidad DLSS.

| Entrada | Estado local | Qué requiere la integración |
|---|---|---|
| Final color | READY | Captura RGBA8 display, format/transfer reales |
| Depth | READY | R32F desde profundidad; near/far e inverted coherentes |
| Motion vectors | PARTIAL | Reproyección de cámara; validar escala/dirección; faltan ciertos movimientos dinámicos |
| HUDless | READY | Captura display anterior al HUD |
| UI/alpha independiente | MISSING | Opcional en API directa; necesario para recomposición UI correspondiente, no inventarlo |
| Optical flow | INTERNAL / UNPROVEN | El descriptor directo no solicita un mapa OF externo; runtime lo calcula. Falta demostrar ruta Ampere compatible |
| Jitter y cámara | READY | Snapshots existentes; verificar convención matrices sin jitter en NGX |
| Render/display size | READY | Inmutables, comprobar formatos y mínimos de feature |
| Timing/reset | READY en Wisteria | Timestamp real y delta para pacing/logging; no inventar un campo timestamp NGX que el helper no publica |
| IDs | READY en Wisteria; adaptación PARTIAL | monotonicFrameId existente; mapear a BackbufferFrameID opcional y epoch/reset, sin repetir IDs |

Fuentes: `docs/FSR_INPUTS_RUNTIME.md`, `NgxFrameGenerationAdapter.java`,
`nvsdk_ngx_defs_dlssg.h` y `nvsdk_ngx_params_dlssg.h`. En x2, API directa declara
multiFrameCount=1 y multiFrameIndex=1. Nuestra clase NGX ya prepara recursos,
matrices, outputs y leases; no ha pasado un gate DLSS-G Ampere. HDR no se anuncia.

## DLSS Super Resolution: bloqueo independiente identificado

El log `logs/runtime/fsr-context-probe-pass.log:93–97` informa ausencia de
`lib/libSuperResolutionNGX+win64+release.dll`. A las 18:55:58, líneas 2674–2678,
DLSS.recreateNgxContext falla tras initializeIfSupported, antes de createDLSS.
El mensaje genérico sobre GPU oculta el fallo de disponibilidad/carga del bridge.
`NgxInitializer.loadBinding` captura UnsatisfiedLinkError y devuelve false sin
registrar detalle. No es evidencia de hardware no soportado.

Confirmaciones actuales de lectura:

- El JAR baseline SR contiene sólo el core nativo; no contiene el JNI NGX.
- `tools/build_sr_native_core.ps1` configuró `SR_NGX=OFF`; fue un build de core.
- El subdirectorio SDK NGX no contiene hoy `include/nvsdk_ngx_vk.h`.
- El runtime SR **sí** existe en la instancia aislada: nvngx_dlss.dll 310.9.0.0,
  firma Authenticode Valid de NVIDIA, SHA256
  `07c7fea19a24c75102bf34d1a4a775640ce323a0b2265d4887b331e62486c44f`.
  Registro `logs/research/dlssg-ampere/local-dlss-runtime.json`.

**Hardware SUPPORTED; proyecto actualmente BLOCKED por packaging/JNI.**
NGX init, capability query, feature creation y evaluación Vulkan quedan PENDING
después de resolver ese primer bloqueo; no se pueden marcar PASS ni atribuir
ahora a recursos incorrectos. No se cambiaron SDK, runtime ni baseline para probarlo.

## Licencias y redistribución

La [licencia NVIDIA RTX SDK](https://github.com/NVIDIA/DLSS/blob/374959484e79a640feaba44c93ac8cfb0a03f5b5/LICENSE.txt)
permite determinados componentes en aplicaciones bajo condiciones (§1–2).
No da autorización general para extraer/recompilar kernels, modificar binarios,
distribuir modelos obtenidos por OTA o eludir limitaciones: §4(a,b,d,e) restringe
ingeniería inversa, modificaciones, circumvention y someter el SDK a copyleft.
Estos términos plantean un conflicto concreto para la adaptación, que debe
aclararse antes de ejecutar o distribuir sus activos. No se ofrece un dictamen
de legalidad ni se deduce permiso porque un DLL esté publicado o firmado.

| Material | Resultado |
|---|---|
| sdli código propio | README declara GPLv3; fuente/licencia completa no publicadas en árbol auditado |
| NVIDIA runtime/models/kernels embebidos | [Notices](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/THIRD_PARTY_NOTICES.txt) los excluye expresamente de GPL; no hay autorización suficiente identificada para nuestra redistribución |
| Streamline fuente común | MIT en license.txt; DLSS-G plugin prebuilt no se vuelve MIT por ello; revisar términos del paquete binario exacto |
| Nukem | GPLv3 LICENSE.md; mantener atribución/fuentes correspondientes si se reutiliza |
| OptiScaler | GPLv3 LICENSE; misma obligación y auditoría de dependencias propias |
| Pipo Vulkan glue / bg3fgvk | MIT declarado para sus fuentes; no relicencia runtime/kernel de otros |
| Modelos NGX/OTA | Procedencia, licencia y versión deben comprobarse por activo; descarga oficial ≠ permiso universal |

El certificado self-signed del proxy principal no es firma oficial NVIDIA.
No se empaquetó ningún binario nuevo. La eventual carga automática debe usar
activos con procedencia, hash, permiso y notices resueltos; nunca selección manual
de DLL, extracción encubierta del proxy o desactivación global de verificaciones.

## Diseño condicionado, sin implementación

Si se obtiene una adaptación Vulkan reproducible y autorizada:

`captura real → DlssgAmpereBackend → NGX directo/adaptador SM86 → VkImage lease → PresentWorker`

Backend `wisteria:dlssg_ampere`, ejecución APPLICATION_MANAGED_ASYNC y selección
independiente del upscaler. Crear DlssgAmpereSession/Adapter/OutputPool nuevos;
apoyarse en interfaces FG y snapshots actuales sin editar bridge AMD/ring/worker.

Capability gate con razones separadas: opt-in experimental; UUID/LUID NVIDIA;
arquitectura real SM86 (SM75 sólo tras otra validación); Windows/driver/HAGS;
bridge ABI y runtime hash/licencia; requisitos extensiones/formats/memory;
init+capabilities+context+evaluate de prueba. Exports presentes o arquitectura
spoofeada no bastan. El backend permanece deshabilitado hasta que esos gates pasen.

Crear outputs offscreen propiedad del pool. Conservar input lease hasta fence de
generación, output lease hasta presentación completa; layouts/ownership de cola
explícitos, ninguna adquisición o vkQueuePresentKHR desde el backend. Reset ante
epoch, resize, recreación de device o discontinuidad; drenar antes de destruir.
Máximo inicial un generado x2. Registrar IDs, timestamps, reset, NGX result,
VkImage, submit/completion, orden y contador únicamente tras éxito real.

**Actualización 2026-10-03:** DLSS SR no es el objetivo ni un gate suficiente para
esta fase. El usuario detuvo implementación/runtime antes de continuar SM86.
La auditoría [DLSSG_MFG_AUDIT.md](DLSSG_MFG_AUDIT.md) confirma desde la guía oficial
una evaluación por índice 1..N, mismo par real y generados antes del real retenido.
Los máximos locales y la conexión NGX Vulkan + backend SM86 siguen sin probarse.
Siguiente investigación: contrato reproducible/ABI y permisos del backend adaptado;
no implementar ni ejecutar todavía. NGX compartido sólo será infraestructura de FG.

Sólo después: harness offscreen para context/create/evaluate/destroy sin
swapchain. Validar historia real A/B y una escena dinámica: hash/diferencia
espacial respecto de **ambos** reales, no sólo VkImage distinto; diferencia de
pixels es necesaria pero no suficiente para probar interpolación correcta.
Respetar OutputDisableInterpolation, reset y counters. Ante driver reset o
device lost detener experimento, conservar logs y no alterar AMD.

Si ese gate pasa, prueba Minecraft aislada breve con x2: mundo/HUD, orden temporal,
sin negro, crash/corrupción y sincronización correcta. Comparar mismo mundo,
resolución, ratio, upscaler, limitadores y warm-up contra AMD: FPS render/present,
timestamps GPU, VRAM, ghosting y pacing. Latencia sin medición instrumental se
debe declarar aproximada/no medida. No hay cifras DLSS-G propias que comparar hoy.

## Integridad final

`tools/record_dlssg_local_evidence.py` verificó exactamente los hashes conservados:

| Artefacto | SHA256 |
|---|---|
| SR fg-backpressure.jar | 20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896 |
| Wisteria fsr-backpressure.jar | ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53 |
| wisteria_fsr_bridge.dll | b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b |

La edición temporal interrumpida no dejó TemporalInterpolator ni registros nuevos
temporal_fg. Actualmente no hay `.git` en los dos árboles; no se recreó metadata
ni se afirmó un nuevo git status limpio. Los hashes son la evidencia de artefactos.
Esta investigación añadió únicamente herramientas, textos de evidencia y docs.
