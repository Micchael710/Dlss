# DLSS-G SM86: integración externa y alternativa D3D12

2026-10-03. Auditoría de código y documentación públicos, sin descargar ni ejecutar
binarios comunitarios. Reverse engineering de GetInfo/Install/SM86Bridge suspendido.
AMD FSR FG x2, PresentWorker, interop y ring conservados.

Estado: auditoría estática CLOSED por instrucción del usuario. Continuación:
[diseño del harness único](DLSSG_EXTERNAL_HARNESS_DESIGN.md). No seguir ABI privada
ni extender esta investigación; gates experimentales permanecen sin ejecutar.

## Resultado principal

Existe un contrato de carga público que evita llamar la ABI privada del backend:
OptiScaler genera el INI y carga el proxy comunitario renombrado. La instalación
privada sigue dentro de ese módulo. No equivale a un backend Vulkan demostrado.
El host loader no recibe un device D3D12 ni exige uno explícitamente; la ruta
comunitaria de ejecución documentada sí es D3D12. Por tanto, DX12 REQUIRED=PARTIAL
para la integración completa: carga sin device explícito, generación validada
por sus autores en D3D12; independencia de los kernels respecto de API desconocida.

## Fuentes fijadas

Descarga exclusiva de textos, Git blob SHA1 comprobado y SHA256 por archivo en
`logs/research/dlssg-external/*/manifest.json` (1.430 textos):

| Proyecto | Commit | Textos |
|---|---|---:|
| ShyVortex/dlss-unlocked | 0798222ff1852574d8987a6fab12955174776371 | 23 |
| ShyVortex/OptiScaler-DLSSNR-PreSR-Multipass | 8bfff2724e0287891d3c636cfede12cb4eca876b | 843 |
| optiscaler/OptiScaler | d63306eae2d3c6aa449d87a9cbaa26d5c83517f6 | 564 |

sdli se mantiene en la revisión ya auditada 9621db573e07ed54f50c15bbb585ed9a7bdfac28.
También revisada la página oficial NVIDIA/DLSS/releases. Sus versiones no prueban
soporte RTX30 ni cambian el resultado stock 0x170 versus 0x190 conservado.

Todas las referencias de código del fork a continuación son a 8bfff272 salvo
indicación. El paquete publicado puede contener binarios de otra revisión: no
se verificó su correspondencia código/binario ni se descargó una release.

## EXTERNAL y AMPERE_MFG_UNLOCK: lectura y efecto

`OptiScaler/Config.cpp` L71 lee `[FrameGen] External`; L74 lee
`[DLSSG] AmpereMfgUnlock`; L189–193 hace que AmpereMfgUnlock implique External=true
y desactiva AdaMfgUnlock. External por sí solo no activa TrySetup del componente.

`OptiScaler/dllmain.cpp` L1893–1910 calcula externalFrameGeneration y aplica
overrides volátiles: FGInput/FGOutput=NoFG, FGNvngxReplacement=None, FGEnabled=false,
fakenvapi deshabilitado, Reflex del juego. No se crea un segundo generador interno.
La excepción Linux puede activar fallback FSR/XeFG (L1912+); Vulkan bajo Proton
para un juego D3D12 no constituye evidencia de NGX Vulkan nativo.

External no significa «cargar cualquier backend y obtener VkImages». Significa
dejar FG al juego/unlocker. Algunos hooks Streamline se omiten (hookInterposer
L1945); hooks del plugin DLSSG todavía se permiten cuando Ampere está activo
(L2224–2226). Es una convivencia de hooks de opciones y generación externa.

OptiScaler upstream d63306 tiene InterpolationCount/OverrideInterpolationCount,
pero no contiene estos campos External/AmpereMfgUnlock ni AmpereMfgLoader en la
fuente capturada. La integración nueva pertenece al fork, no al upstream sin cambios.

## Secuencia pública de carga

1. DLSS-Unlocked prepara el paquete, no implementa los kernels.
   `build-optiscaler.ps1` L13 fija origen predeterminado sdli1995; L325–331 obtiene
   su `version.dll` y lo guarda como `dlssg_sm86.dll`. L428–440 configura flags.
   El instalador ISS L114–115 copia el subdirectorio y L171–186 configura ambos flags.
2. El proceso carga OptiScaler como proxy; configura el estado External.
3. `dllmain.cpp` L2201 lanza getGpuInfo; L1740–1768 enumera GPU y llama TrySetup
   desde un worker después del attach. No hace esta carga dentro de DllMain.
4. `framegen/dlssg/AmpereMfgLoader.cpp` L435–485 exige flag, NVIDIA y Turing/Ampere,
   excluye Ada unlock simultáneo; usa arquitectura NVAPI y también heurísticas de nombre.
5. L499–532 busca candidatos relativos al módulo/override: preferentemente
   `OptiScaler/dlssg_sm86/dlssg_sm86.dll`, acepta también `version.dll` y rutas legacy.
   Turing selecciona según presencia de SM75; puede caer a 310.1. Ampere usa root.
6. L618–635 sobrescribe `dlssg_sm86.ini` junto al DLL; configura runtime, router,
   formato de kernel y límite según variante. Debe existir permiso de escritura.
7. L697–718 carga mediante NtdllProxy::LoadLibraryExW_Ldr y fallback LoadLibraryW;
   conserva HMODULE. No GetProcAddress de DlssgMod_GetInfo/Install/SM86Bridge_Install.
8. Para sdli, la inicialización del proxy sucede al cargarlo, según documentación
   previa de sdli; el proxy contiene el loader/runtime/backend y instala hooks.
   Para otra variante SilyNoMeta, L755–778 llama opcionalmente InitializeASI o
   DLSSG_UniversalProxy. No asumir que todas las variantes arrancan sólo por LoadLibrary.
9. Juego/Streamline/NGX realizan sus llamadas FG. Cargar el módulo no crea una
   feature, no produce outputs ni configura inputs por sí solo.

No hay barrera pública que certifique «backend instalado antes de slInit/NGX»:
el worker arranca asincrónicamente; LoadLibrary retornado sólo prueba carga.
Para un harness futuro, ordenar explícitamente carga antes del init y comprobar
logs de instalación, mínimo del snippet, kernel_create y Evaluate. No ejecutar ahora.

## Host contract público

| Elemento | Evidencia / requisito |
|---|---|
| Nombre/ruta | DLL absoluta encontrada en las rutas candidatas; INI se llama dlssg_sm86.ini junto al módulo. Renombrar version.dll a dlssg_sm86.dll es la estrategia del paquete. |
| Configuración | INI generado con Enabled=1, Optimized, MaxGeneratedFrames, Router, KernelImage, Preset, logging y Mode=Bundled. No variables especiales Windows requeridas por TrySetup. |
| Orden | INI antes de carga; adaptación antes del primer gate/feature del host. La implementación worker no garantiza ese orden frente a todos los juegos. |
| Callbacks | sdli no necesita callbacks privados del host en esta fuente. Sily ofrece control opcional publicado por el fork: RequestControl/SetDisplayTarget/RequestUI y ASI init. No confundirlos con la ABI privada pausada. |
| Streamline | No se pasa un objeto Streamline ni se exige módulo presente para cargar. El paquete proporciona SL2.14.1 y presupone un juego con integración FG; el flujo completo normalmente lo usa. NGX directo público es otra ruta por demostrar. |
| D3D12 device | TrySetup() no acepta device/queue/swapchain. getGpuInfo sólo consulta capacidades D3D12 si ya hay módulo D3D12 cargado (L1762–1764); esa consulta puede crear device temporal. No hay dependencia obligatoria de ese device en la firma del loader. |
| NVIDIA runtime | Bundled contiene runtime y backend correspondientes; carga NGX/driver/NVAPI y kernels compatibles posterior a los hooks. Un DLL de runtime cualquiera no sustituye ese par. |
| Lifecycle | s_setupAttempted evita reintentos; s_hModule se conserva. No se encontró Shutdown/FreeLibrary del módulo en AmpereMfgLoader. No contrato de hot unload seguro. Mantenerlo por vida del proceso en cualquier experimento futuro. |
| Condición PASS | DllLoaded es carga, no disponibilidad verificada, instalación de kernels, Evaluate ni generated output. |

La variante Sily tiene en el header un comentario «mode3 Adaptive Vulkan».
ResolveControlModeAndMultiplier L302–320 sólo emite modos 0,1,2; el comentario no
demuestra soporte Vulkan de sdli, ni offscreen, ni ejecución en RTX3050Ti.

## DX12 frente a Vulkan

La carga externa elimina la necesidad del host de hablar con la ABI privada.
No demuestra que SM86Bridge instale kernels en todas las APIs del snippet.
sdli README L41/L45 sigue describiendo ejecución Windows x64/D3D12 y Vulkan
pendiente. No existe un branch público auditado que muestre instalación SM86
Vulkan como consecuencia de esa carga. CAN BACKEND PATCH NGX BEFORE VULKAN INIT=UNKNOWN.
Es posible *ordenar la carga* antes de init Vulkan; distinto de confirmar la adaptación.

El fork permite paso de Evaluate Vulkan nativo en `inputs/NVNGX_DLSS_Vk.cpp`
L1021–1031. Eso delega a NGX; no instala kernels SM86.
Peor aún, `NVNGX_Parameter.cpp` L816–857 pone Available=1/FeatureInitResult=1
si Ampere está seleccionado, también para API::Vulkan. Calcula max anunciado
desde INI y un ceiling3/5 por variante, **no desde una query hardware original**.
`inputs/NVNGX_DLSS_Dx12.cpp` L1001–1016 también puede declarar requirements Supported.
Estos cambios son anuncios hacia el cliente, no nuestro gate de prueba real.

Para una futura comparación, guardar stock original, adapted original sin
reescritura de OptiScaler, y reported después del hook. No importar sus ceilings
al capability probe ni marcar PASS por Available forzado.

## MFG: separación de cuatro niveles

| Nivel | Ruta pública | Qué se puede concluir |
|---|---|---|
| UI/INI interno | Config.cpp L363–373; upstream Config.h L629–632 separa propia SL y override del juego. | count es generaciones extra; valores del parser hasta6 no prueban techo operativo. |
| Streamline interno | DLSSG_Dx12.cpp L405–431: InterpolationCount → clamp a máximo → options.numFramesToGenerate → SL SetOptions. | InterpolationCount=5 solicita cinco extras sólo si esa instancia interna ejecuta. External la evita. |
| Streamline externo | OverrideInterpolationCount → dlssgOptionsState → hooks/Streamline_Hooks.cpp L1270–1277 → numFramesToGenerate, clamp al máximo observado. | Para External, opción del juego u override; InterpolationCount solo no obliga al generador externo. |
| NGX/backend | Direct hook D3D12 L1236–1264 modifica MultiFrameCount; capabilities arriba pueden ser anuncios. | No se ve aquí un loop emisor de indices1..N: el juego/plugin/runtime debe producirlo. Set count no crea por sí solo Evaluate adicionales. |

En la ruta Sily opcional, RequestControl modo fijo usa multiplicador=count+1
(AmpereMfgLoader.h L302–314); MaxGeneratedFrames es un ceiling independiente.

La auditoría oficial anterior en DLSSG_MFG_AUDIT.md demuestra el contrato NGX
por intervalo: count=N constante, Evaluate index1..N, inputs conservados,
outputs preservados. Los helpers D3D/Vulkan pasan los campos. No hemos observado
esa secuencia ejecutada por este paquete: plugin/backend cerrados, cero runs.

| Multiplicador | Count | Indices oficiales requeridos | Nuestro RTX3050Ti Vulkan |
|---|---:|---|---|
| 2x | 1 | 1 | NOT_TESTED adaptado; stock bloqueado |
| 3x | 2 | 1..2 | NOT_TESTED |
| 4x | 3 | 1..3 | NOT_TESTED |
| 5x | 4 | 1..4 | NOT_TESTED; requiere ceiling real >=4 |
| 6x | 5 | 1..5 | NOT_TESTED; requiere ceiling real >=5 |

sdli documenta ceilings 310.1→3 y310.9→5, con pruebas D3D12 en otras GPUs.
No son un MultiFrameCountMax medido en nuestra GPU ni prueba de port Vulkan.

## PLAN B: sidecar D3D12, diseño condicionado

Los mecanismos de memoria/fence existen oficialmente. Factibilidad del
transporte=YES a nivel de API, sujeta a consultas del driver/formato.
Factibilidad de FG SM86 offscreen=UNKNOWN hasta demostrar que el DLL sirve
a NGX D3D12 directo sin exigir un swapchain/hook de presentación.

Propuesta: proceso D3D12 auxiliar aislado, no OptiScaler completo dentro de la
JVM. Conserva los hooks comunitarios fuera de la baseline AMD. Render/present
siguen en Vulkan; el auxiliar sólo escribe recursos compartidos y señala fences.

```text
Wisteria Vulkan: inputs reales -> copia GPU a recursos compartidos -> fence input-ready
D3D12 sidecar: wait -> NGX DLSS-G offscreen -> G1 compartido -> fence output-ready
Wisteria Vulkan: wait -> lectura/copia a VkImage con lease -> PresentWorker
```

1. Igualar DXGI adapter LUID con VkPhysicalDeviceIDProperties.deviceLUID; no
   elegir automáticamente la iGPU del portátil. Consultar imports por formato,
   usage/tiling y semáforo mediante consultas externas Vulkan.
2. Comenzar con recursos committed D3D12 creados compartibles, handles NT de
   CreateSharedHandle. Importar como D3D12_RESOURCE_BIT en Vulkan, con imagen
   declarada external desde su creación y dedicated allocation cuando corresponda.
   No reutilizar sin más una imagen OpenGL/Vulkan ya asignada ni reinterpretar
   un handle OPAQUE_WIN32 como D3D12_RESOURCE.
3. D3D12_HEAP_BIT es alternativa para shared heaps/placed resources; exige
   verificar offsets, alignment, tamaño y compatibility. No elegirlo por defecto.
4. Compartir ID3D12Fence y usar D3D12_FENCE_BIT importado como timeline semaphore.
   Valores monotónicos separados para input/output/reuse; ownership release/acquire
   externo e image layouts/resource states correctos. No uso concurrente sin wait.
5. Preservar A/B, depth/MV/HUDless/camera/timing; revisar formatos y escalas.
   Outputs con lease hasta que PresentWorker termine; B retenido hasta GN.
6. Negociar generación de slots/resize; no liberar recursos hasta GPU completion.
   IPC handles mediante duplicación y cierre de handles NT; referencia de memoria
   y handles tienen lifetime distintos. Detectar sidecar crash/device lost.
7. Empezar por x2 y medir transporte, FG GPU time, VRAM y pacing. No añadir MFG
   antes de output real distinto de A/B. Sin CreateSwapChain ni Present en sidecar.

Referencia pública ya existente: el fork `upscalers/IFeature_VkwDx12.cpp`
L484–573 comparte recursos D3D12/importa D3D12_RESOURCE_BIT; L2250–2292 importa
fences D3D12 como semáforos timeline. Es infraestructura de upscaling, no prueba
de FG SM86 sidecar, ni código que deba copiarse íntegro.

Referencias oficiales:

- [Tipos de memoria externa](https://docs.vulkan.org/refpages/latest/refpages/source/VkExternalMemoryHandleTypeFlagBits.html).
- [Import Win32](https://docs.vulkan.org/refpages/latest/refpages/source/VkImportMemoryWin32HandleInfoKHR.html).
- [Fence D3D12 en Vulkan; preferencia por timeline](https://docs.vulkan.org/refpages/latest/refpages/source/VkD3D12FenceSubmitInfoKHR.html).
- [CreateSharedHandle](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createsharedhandle).

## Ruta recomendada y límites

Primero cerrar estáticamente la identidad/licencia del módulo elegido y su
contrato externo; ya no necesitamos adivinar Install. Siguiente experimento,
cuando se autorice ejecución comunitaria: harness aislado carga+INI ordenados,
consulta original NGX Vulkan y logs de backend. Si no hay kernels Vulkan,
probar D3D12 offscreen antes de construir transporte sidecar. Ninguna ejecución
autorizada ni realizada en esta fase.

No instalar el paquete completo en Minecraft. El script empaqueta otros parches
NR/SmoothMotion/forwarders irrelevantes, incluso URLs de terceros; no son
dependencias probadas de DLSS-G ni deben heredarse en Wisteria.
Las licencias del código y runtime/kernels son distintas; THIRD_PARTY_NOTICES
del paquete contiene referencias históricas Native0.2.3, no una verificación
del bundle actual. El renombrado y los avisos no conceden por sí solos permiso
de redistribución. No empaquetado automático.
