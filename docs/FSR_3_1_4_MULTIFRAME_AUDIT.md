# Auditoría temporal FSR FG 3.1.4, 3.1.5 y 3.1.6

Fecha: 2026-10-02. Investigación de código; sin ejecución, compilación, migración
ni modificaciones de Super Resolution, Wisteria, bridge o configuración runtime.

**C) 3.1.4 ONLY SUPPORTS ONE INTERPOLATED FRAME PER DIRECT DISPATCH**

El provider de 1.1.4 genera un único resultado en el punto medio del intervalo.
El swapchain Vulkan tampoco ofrece una ruta multiframe. Los providers analíticos
3.1.5 y 3.1.6 inspeccionados conservan este comportamiento. La capacidad de cuatro
entradas del descriptor público no demuestra cuatro interpolados implementados.

## Procedencia y reproducibilidad

| Paquete | Revisión exacta | Identificación FG |
| --- | --- | --- |
| FidelityFX SDK v1.1.4 | `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55` | Baseline FSR FG 3.1.4; versión interna del efecto 1.1.3 |
| SDK v2.0.0 | `f4c1da8e92f3fe563b5c28c44e6267ce6b6b8eb2` | `FFX_FRAMEINTERPOLATION_VERSION_* = 3,1,5` |
| SDK v2.1.0 | `0836aa7f24058b3f8d035a88c0774457d78fc98e` | `FFX_FRAMEINTERPOLATION_VERSION_* = 3,1,6` |

1.1.4 se extrajo selectivamente del archivo oficial ya conservado, sin sustituir
el SDK ni sus binarios. SHA256 del ZIP:
`0216556bfb0e243cec30004a2a98d38f4e3f7406cb7938e3c1b85c758e95d952`.
Origen: [release oficial 1.1.4](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4).

Los textos posteriores proceden exclusivamente de `raw.githubusercontent.com`
del repositorio oficial GPUOpen, fijados a los commits de la tabla. No se
descargaron/ejecutaron DLL posteriores ni se añadió ningún remote al proyecto.
Los árboles oficiales descargados no están truncados. Los manifiestos incluyen
URL, SHA256 y tamaño de cada texto; todos los hashes descargados se verificaron.

Evidencia reproducible en `D:/ProjectDllsss/logs/research/`:
`fsr-1.1.4/audit-manifest.json`, `v2.0.0/audit-manifest.json`,
`v2.1.0/audit-manifest.json`, y `comparisons/summary.json` con los diffs adyacentes.
Herramientas de investigación: `tools/audit_fsr_local_sources.py`,
`tools/audit_fsr_later_sources.py`, `tools/compare_fsr_audit.py`.

## Traza exacta 1.1.4: descriptor → provider → dispatch → shader

Todos los archivos de esta sección están bajo
`D:/ProjectDllsss/logs/research/fsr-1.1.4/`; números de línea del texto original.

| Etapa | Archivo y línea | Comportamiento observado |
| --- | --- | --- |
| API pública | `ffx-api/include/ffx_api/ffx_framegeneration.h:90–91` | Array `outputs[4]` y campo `numGeneratedFrames`; capacidad del descriptor, no garantía del provider. |
| Adaptador callback | `ffx-api/src/ffx_provider_framegeneration.cpp:255–259` | Copia `numInterpolatedFrames` a `numGeneratedFrames` y copia los cuatro recursos. No genera imágenes en esta copia. |
| Provider directo | Mismo archivo, `453` | Convierte solamente `desc->outputs[0]` a `fiDispatchDesc.output`. |
| Ejecución | Mismo archivo, `519` | Una llamada a `ffxFrameInterpolationDispatch`, después de un dispatch de optical flow. No bucle por el número solicitado. |
| API interna FI | `sdk/include/FidelityFX/host/ffx_frameinterpolation.h:247–282` | Un único `output` en línea 255; no array ni fase por salida. |
| Registro UAV | `sdk/src/components/frameinterpolation/ffx_frameinterpolation.cpp:1049` | Registra ese único recurso como UAV de salida. |
| Pases | Mismo archivo, `1109–1190` | Setup, reconstrucción, campos de movimiento, máscara de disoclusión, interpolación e inpainting sobre ese resultado. |
| Actualización de historia | Mismo archivo, `1197–1210` | Programa la copia del color actual al recurso anterior. El dispatch no es una función pura reutilizable con otra fase. |
| Otro wrapper oficial | `sdk/src/components/fsr3/ffx_fsr3.cpp:324,360` | También selecciona `callbackDesc->outputs[0]` y llama una vez a FI. |

En el provider directo, la única aparición de `numGeneratedFrames` es su
asignación en el adaptador callback; no determina un bucle, un índice de salida o
un parámetro shader. Cambiar ese entero a 2, 3 o 4 no genera las salidas adicionales.
Tampoco debe interpretarse un código de retorno de éxito como prueba de que se
escribieron todas las entradas solicitadas: esta ruta sólo consume la primera.

`MAX_QUEUED_FRAMES = 2` en el provider sirve para almacenar prepares/callbacks por
frameID, no para producir dos interpolados del mismo intervalo.

## Posición temporal y supuestos de los shaders

Archivos bajo `sdk/include/FidelityFX/gpu/frameinterpolation/`:

| Lugar | Línea | Evidencia temporal |
| --- | --- | --- |
| `ffx_frameinterpolation_game_motion_vector_field.h` | 48–49 | Movimiento del juego multiplicado por `0.5f`; ubicación interpolada calculada con ese medio vector. |
| `ffx_frameinterpolation_optical_flow_vector_field.h` | 31,52 | Optical flow multiplicado por `0.5f`; splatting hacia la posición intermedia. |
| `ffx_frameinterpolation_reconstruct_previous_depth.h` | 60 | Reconstrucción de profundidad interpolada con `fMotionVector * 0.5f`. |
| `ffx_frameinterpolation.h` | 118–122 | Mezcla de colores comienza con `t = 0.5f`; disoclusión modifica por píxel el peso de fuentes válidas. |
| Mismo shader | 141–155 | Optical flow comienza con `ofT = 0.5f`, con fallback a una fuente si falta la otra. |

La geometría se interpola al punto medio. Los ajustes de color por disoclusión
no son una fase global configurable: elegir una fuente válida en un píxel no
desplaza todo el frame a 1/3 o 2/3 del intervalo. Otros `0.5` corresponden a centros
de píxel o cálculos geométricos; sustituirlos globalmente sería incorrecto.

La estructura interna de dispatch y los constant buffers CPU/HLSL/GLSL no
contienen una fracción temporal, timestamp destino, índice temporal de output ni
una lista de fases. En `ffx_frameinterpolation.cpp:953`, `frameTimeDelta` se copia
a `constants.deltaTime`; ese campo sólo aparece declarado en los shaders
inspeccionados, no controla estos factores de interpolación. `NumInstances`
tampoco se usa como número de interpolados: aparece declarado, sin una ruta que
lo llene desde `numGeneratedFrames` o que itere sobre salidas. `Mode` se fija a 0.

`frameID` sirve para recursos/historia y reset por discontinuidad
(`ffx_frameinterpolation.cpp:931–944`), no como fase. Variar IDs o timestamps para
despachar repetidamente no constituye una ruta oficial multiframe: se cambia la
historia del algoritmo, y sigue faltando una fase objetivo diferente. Duplicar
contextos tampoco añade ese parámetro. No se probó ni implementó ninguna de estas
maniobras.

## Swapchain Vulkan: quién llena el contador

`sdk/src/backends/vk/FrameInterpolationSwapchain/FrameInterpolationSwapchainVK.cpp`:

- `2033`: el swapchain llena únicamente `desc.outputs[0]` con su imagen actual.
- `2036`: el propio swapchain asigna `desc.numInterpolatedFrames = 1` antes de
  invocar `frameGenerationCallback`.
- `2098–2101`: comprobar `numInterpolatedFrames > 0` decide si existe una imagen
  interpolada que presentar; no itera por N resultados.
- `2019`: la alternativa de command list registrada también retorna una sola
  `interpolationOutput()`.
- `1943–1948`: incluso `interpolationOutput(int index)` selecciona el índice de
  ring actual, no una fase temporal. Los buffers del ring rotan entre intervalos.
- `1267`: el pacing estima medio intervalo para la presentación; esto modifica
  cuándo se presenta el interpolado, no la imagen producida por los shaders.

En dispatch directo el llamador llena `numGeneratedFrames`. En callback, AMD
inicializa el contador del descriptor del swapchain y el provider lo copia.
Ninguna de las rutas oficiales auditadas en 1.1.4 produce más de un interpolado
temporalmente distinto entre el mismo par de frames reales. Pasar al swapchain
AMD no resuelve x3 y además cambiaría la propiedad de presentación requerida.

## Comparación fijada a 3.1.5 y 3.1.6

Rutas posteriores bajo `Kits/FidelityFX/framegeneration/`:

| Prueba | 3.1.5, SDK v2.0.0 | 3.1.6, SDK v2.1.0 |
| --- | --- | --- |
| Descriptor público | `include/ffx_framegeneration.h:90–91`: mismo array/contador | Mismo archivo `95–96`: mismo array/contador |
| Provider directo | `fsr3/internal/ffx_provider_fsr3framegeneration.cpp:486,552`: output0 y una llamada FI | Mismo archivo `604,668`: output0 y una llamada FI |
| Dispatch FI | `fsr3/include/ffx_frameinterpolation.h`: un solo output, sin fase | Misma estructura de dispatch, sin nuevas salidas/fases |
| Swapchain DX12 | `fsr3/dx12/FrameInterpolationSwapchainDX12.cpp:1354,1357`: output0 y numInterpolatedFrames=1 | Mismo archivo `1811,1814`: output0 y numGeneratedFrames=1 |
| Shaders temporales | Siguen siendo de punto medio | Siguen siendo de punto medio |
| Ruta Vulkan FG en árbol oficial | No encontrada en árbol completo | No encontrada en árbol completo |

Los cuatro shaders de interpolación, campo del juego, campo optical flow y
reconstrucción de profundidad sólo cambian el copyright entre 1.1.4 y 3.1.5.
Entre 3.1.5 y 3.1.6 su texto normalizado es idéntico; también lo es el callback
HLSL del constant buffer. No aparece soporte multiframe temporal en estas
transiciones.

Clasificación de cambios relevantes al contrato y la ruta inspeccionada:

| Cambio | Clasificación | Implicación |
| --- | --- | --- |
| 3.1.5: query de memoria GPU antes de crear contexto, nuevos tipos/includes | API_ONLY en el contrato; PROVIDER_CHANGE y FRAME_INTERPOLATION_CHANGE en la implementación de la consulta | No añade fases ni outputs ejecutados. |
| 3.1.5: reorganización del SDK/backend 2.0, recursos, validación de formatos y diagnósticos | PROVIDER_CHANGE, FRAME_INTERPOLATION_CHANGE | Cambios reales de integración; no multiframe. |
| 3.1.5: fixes de command lists con >2 frames de latencia y deadlock alt-tab | SWAPCHAIN_ONLY, DX12_ONLY para la ruta publicada | Frames en vuelo/latencia no significan varios interpolados por intervalo. |
| 3.1.6: descriptor de versión, PrepareV2 y cámara incorporada | API_ONLY para forma/versionado; PROVIDER_CHANGE para conversión/cámara/reset | Nuevo reset de prepare se combina realmente con reset de dispatch y discontinuidad, sin fase temporal. |
| 3.1.6: compatibilidad entre ABI de callback/swapchain | PROVIDER_CHANGE, SWAPCHAIN_ONLY; implementación swapchain DX12_ONLY | Adapta estructuras antiguas/nuevas; sigue solicitando un resultado. |
| 3.1.6: memoria/heap placement y validación de creación | FRAME_INTERPOLATION_CHANGE | No cambia las fórmulas temporales de los shaders comparados. |
| Contratos de cámara/reset y lógica CPU independientes del API gráfico | PORTABLE_TO_VULKAN como posibilidad de ingeniería, no soporte oficial ni port realizado | Requerirían adaptar tipos/backend/ABI; no habilitan x3–x5. |

La clasificación cubre los cambios relevantes a esta auditoría; no pretende
enumerar todos los cambios de todos los efectos del SDK. No se debe confundir
FSR Upscaling 3.1.5 con FSR Frame Generation 3.1.5 ni mezclar providers analíticos
3.x con FG ML 4.x. La referencia a Vulkan en comentarios genéricos de la API de
memoria tampoco demuestra que se publique un provider Vulkan en estos tags.

Fuentes oficiales, además del código local fijado:
[provider 3.1.5](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/f4c1da8e92f3fe563b5c28c44e6267ce6b6b8eb2/Kits/FidelityFX/framegeneration/fsr3/internal/ffx_provider_fsr3framegeneration.cpp#L486),
[provider 3.1.6](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/0836aa7f24058b3f8d035a88c0774457d78fc98e/Kits/FidelityFX/framegeneration/fsr3/internal/ffx_provider_fsr3framegeneration.cpp#L604),
[shader 3.1.6](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/0836aa7f24058b3f8d035a88c0774457d78fc98e/Kits/FidelityFX/framegeneration/fsr3/include/gpu/frameinterpolation/ffx_frameinterpolation.h#L118),
[notas oficiales SDK 2.0.0](https://gpuopen.com/manuals/fsr_sdk/whats-new/version_2_0_0/),
[notas oficiales SDK 2.1.0](https://gpuopen.com/manuals/fsr_sdk/whats-new/version_2_1_0/).

## Decisión y siguiente ruta

| Ruta propuesta | Evaluación |
| --- | --- |
| Portar multiframe desde provider 3.1.5/3.1.6 a Vulkan | No hay tal comportamiento en esos providers. Portarlos daría otra integración x2, no fases distintas para x3–x5. |
| Usar otra ruta oficial de 3.1.4 | Direct, wrapper FSR3 y swapchain Vulkan auditados generan uno; ninguna resuelve el objetivo. |
| Mantener 3.1.4 x2 | Recomendación actual: soportada por código y baseline validada, conserva PresentWorker como dueño de presentación. |

Para x3–x5, el próximo requisito de investigación sería identificar un backend
oficial con fases múltiples explícitas y evidencia de imágenes temporalmente
distintas del mismo par real, así como soporte Vulkan o un port viable. No se
ha identificado esa ruta entre las versiones solicitadas. No recomendar una
migración ni cambios de scheduler antes de demostrarla.

El riesgo de avanzar sólo por `outputs[4]` es presentar imágenes no escritas,
repetidas o con historia incorrecta y reportar multiplicadores falsos. Introducir
fases nuevas en estos kernels sería un cambio de algoritmo que exigiría derivar
y validar reconstrucción, splatting, disoclusión, mezcla, optical flow e historia;
no un parche de contador o un reemplazo aislado de `0.5`.

## Integridad de la baseline conservada

`AMD_FSR_FG_X2 = PASS` permanece como gate validado anteriormente; no se repitió.
Repos limpios, commits sin cambios:
SR `e98b3fdb907a84f9b9fc0d5f6f45f7846158ba4c`,
Wisteria `322ff7acc733c3699dd161a36f143584afcc157f`.
Ambos sin remotes. SHA256 verificados de nuevo, sin reconstruir:

- JAR SR: `20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896`.
- JAR Wisteria: `ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53`.
- Bridge: `b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b`.

Sin cambios de bridge, PresentWorker, lifecycle, ring fix, scheduler x2 o
configuración; sin x3/x4/x5, push, PR ni acceso de escritura a `.minecraft` real.
