# Comparación del proyecto de tu amigo con nuestra baseline

2026-10-03. Carpeta examinada: `D:/ProjectDlssmiAmigo`.
Nuestra copia: `D:/ProjectDllsss/superresolution` y `D:/ProjectDllsss/wisteria`.
Revisión estática de fuentes, configuración y contenido de los JAR suministrados.
No ejecuté sus binarios, scripts, Gradle ni Minecraft. No modifiqué ninguna de las
dos implementaciones. El PRD de su carpeta se trató como especificación de su
proyecto, no como instrucciones nuevas para ejecutar o implementar aquí.

**Sí se puede mejorar el suyo. Mantendría nuestra baseline como base estable y
aprovecharía ideas concretas de su proyecto mediante cambios pequeños.**
La diferencia principal es que su provider llamado FSR utiliza un interpolador
propio con fase variable. Nuestra ruta x2 ejecuta el SDK AMD oficial. Su trabajo
no demuestra que el provider AMD 3.1.4 soporte más de un interpolado.

## Diferencias que afectan a la decisión

| Aspecto | Nuestra baseline | Proyecto del amigo |
| --- | --- | --- |
| Generación | JNI propio → FidelityFX SDK 1.1.4, FG 3.1.4, Vulkan | WisteriaNative → shader propio `fsr_fg_compute.comp`, Vulkan |
| Fase temporal | Punto medio implementado por AMD | Push constant `lambda`; bucle k/(N+1) en Java |
| Capacidad publicada | Un generado, x2 validado | Constante de cinco generados, permite hasta x6 |
| Evidencia runtime | Run conservado de 4912 reales + 4912 generados; mundo/HUD y orden revisados | JARs presentes; no encontré logs de una prueba comparable en la copia examinada |
| Captura llena | Backpressure hasta submission antes de reutilizar | Lanza excepción si no hay slot reutilizable |
| Timing | Timestamp/ID/epoch/sizes/metadata inmutables capturados del frame real | Intervalo tomado de `System.nanoTime()` en worker FG, con fallback 1/60 |
| Disponibilidad | Bridge/context/session/capabilities reales | DLL cargada; cambio de capabilities con `Unsafe` |
| API SR | Paquetes `registry.framegeneration` y contratos actuales | Paquetes de registry distintos y otra generación de presentación |

Las diferencias amplias de fuentes no deben atribuirse todas a cambios de tu
amigo: las dos copias también tienen diferencias de versión/estructura upstream.
Por ello no recomiendo reemplazar carpetas ni mezclar sus JAR con los nuestros.

## Hallazgos prioritarios y correcciones propuestas

### 1. P1 — La ruta llamada FSR no ejecuta FidelityFX Frame Generation

Evidencia: `D:/ProjectDlssmiAmigo/Wisteria/native/cpp/src/Fsr3Pipeline.cpp:324–333`
envía push constants y ejecuta `vkCmdDispatch` de su pipeline compute.
`native/cpp/src/fsr_fg_compute.comp:6,16,140` contiene una fase propia y una mezcla
temporal. `native/cpp/CMakeLists.txt` compila esos archivos y el bridge; la ruta
revisada no crea ni despacha contextos FidelityFX. Los binarios de FSR presentes
en Super Resolution pueden servir a upscaling; no cambian esta traza de FG.

Su bucle puede solicitar imágenes con fases distintas en ese algoritmo propio.
Eso es distinto de duplicar necesariamente el mismo resultado, pero la calidad
y la corrección temporal no están demostradas por la existencia del bucle.
No contradice la auditoría de los providers oficiales 3.1.4/3.1.5/3.1.6.

Aclaración tras la observación del usuario sobre la DLL AMD: sí existe una ruta
de carga de `amd_fidelityfx_upscaler_dx12.dll` en su Super Resolution,
`native/cpp/SRNativeFSR4/src/ffx_api_upscale.cpp:334`. Crea un descriptor de
upscaling (`:36`) y despacha `ffxDispatchDescUpscale`, con tipo UPSCALE (`:525–526`).
Esta evidencia identifica el uso para upscaling, no una llamada FG del provider
Wisteria analizado. No se ha observado esa carga en runtime ni se ha identificado
otra DLL AMD indicada por el usuario; la conclusión no debe generalizarse a que
todo su proyecto carece de AMD/FidelityFX.

Mejora: publicar esa ruta como interpolación experimental propia, en un provider
independiente con un nombre que no afirme usar AMD FSR FG. Si el objetivo es AMD
oficial, usar nuestra integración x2. No intentar convertir su bucle lambda en
multiframe AMD únicamente reescalando vectores/delta.

### 2. P1 — JNI informa éxito incluso cuando no se graban comandos

`Fsr3Pipeline.cpp:242–253` retorna sin hacer dispatch ante handles inválidos o un
fallo de `CreateComputePipeline`. Su método devuelve `void`.
`native/cpp/src/wisteria_jni.cpp:91–92` llama a ese método y devuelve 0 siempre.
Java interpreta ese código como éxito y puede entregar outputs que no fueron
escritos. Es un defecto verificable por flujo de código; no afirmo haber observado
un frame negro en runtime.

Mejora: devolver un resultado nativo real; propagar errores de creación/registro
de comandos hasta Java; abortar el lease y presentar sólo el frame real ante
fallo. Añadir query de ABI/capabilities y una prueba que induzca fallo de creación.

### 3. P1 — Se fuerzan capabilities en vez de evaluarlas

`common/src/main/java/org/ireallywanttosleep/wisteria/backend/VulkanAsyncFoundationBridge.java:137–148`
construye capabilities con `available=true`, conserva los otros campos y los
inserta con `UNSAFE.putObject`; no demuestra que la nueva combinación sea válida.
El backend FSR llama a esa ruta desde sus consultas de disponibilidad/dependencias.
`backend/fsr/FsrFrameGenerationAdapter.java:73–78` basa disponibilidad en DLL cargada
y publica cinco outputs mediante una constante, antes de validar el pipeline.

Mejora: consultas sin efectos secundarios, basadas en requisitos de colas,
semáforos, formatos, pipeline y backend; cambiar de provider mediante una API
pública de SR que drene el trabajo y actualice el estado. El rechazo debe mostrar
una razón útil. No importar este hack a nuestra baseline.

### 4. P1 — El formato del shader y el de la imagen pueden no coincidir

`native/cpp/src/fsr_fg_compute.comp:16` fija el storage output a `rgba16f`.
`backend/fsr/FsrFrameGenerationAdapter.java:431` asigna a outputs el formato del
backbuffer, sin exigir RGBA16F. Con un formato diferente, el contrato no queda
garantizado y puede haber incompatibilidad entre shader e image view.

Mejora: validar/restringir el formato requerido, o separar outputs generados
RGBA16F de la copia real en formato origen y hacer la conversión/presentación
compatible. No cambiar a RGBA16F todas las imágenes sin revisar `vkCmdCopyImage`.
Este riesgo depende del formato runtime; falta reproducirlo en una prueba aislada.

### 5. P1 — Sigue presente el fallo de saturación del capture ring

`D:/ProjectDlssmiAmigo/superresolution/common/src/main/java/io/homo/superresolution/common/presentation/capture/CaptureFrameRing.java:117–122`
lanza `No reusable capture slot is available` cuando la captura adelanta al worker.
Nuestra copia espera submission para permitir reutilización segura; ese cambio
se verificó durante el cierre de x2.

Mejora: portar el comportamiento y su contrato de lifecycle como un cambio conjunto,
con las pruebas de backpressure. No liberar buffers antes de la GPU ni aumentar
solamente el tamaño del ring. Es una mejora concreta de nuestra versión para la suya.

### 6. P2 — Timing, resets y calidad necesitan un contrato más preciso

`backend/fsr/FsrFrameGenerationAdapter.java:125–133` mide el intervalo del worker,
que incluye retrasos de scheduling, en lugar de conservar el intervalo de los
frames fuente. En el shader `frameDeltaTime` sólo está declarado; no controla
la interpolación. Nuestro snapshot del frame real ofrece una base mejor para
pacing, registros y discontinuidades.

El shader identifica la mano con `depth <= 0.56` (`:71`), una heurística que no
identifica de forma universal la mano; puede afectar a geometría cercana. Hace
una búsqueda de 7×7 offsets (`:90–92`) y un vecindario 3×3 por píxel, por salida.
El coste crece con resolución y número de generados; su rendimiento no se puede
deducir de la existencia de cinco slots ni de un contador.

Mejora: metadata inmutable desde render, epochs de discontinuidad, máscara de mano
real si existe, procedencia/escala de motion vectors y transferencia de color
explícitas. Medir coste GPU y artefactos con escenas de movimiento/oclusiones;
no presentar las heurísticas como garantías de calidad AMD.

### 7. P2 — Reutilización de descriptors y cierre requieren demostrar ownership

`native/cpp/include/Fsr3Pipeline.h:55` reserva 128 descriptor sets;
`Fsr3Pipeline.cpp:258–259` rota por módulo. Esa ruta nativa no asocia la reutilización
a un fence. Debe demostrarse un límite suficiente de trabajo en vuelo desde el
scheduler, o añadir lease por submission/completion. No concluyo que haya una
carrera observada: el límite Java podría evitarla, pero el contrato nativo no la
garantiza por sí solo.

La selección de pipeline global en JNI y su destrucción también deben coordinarse
con device/thread/GPU vivos. Los cambios de provider no deben cerrar recursos
desde una consulta de disponibilidad ni confiar en excepciones ignoradas.

## Ideas aprovechables

- **Protección de render de portales:** su SR añade guards de
  `ImmersivePortalsCompat.isRenderingPortal()` a captura, dispatch y otras rutas.
  Es una idea útil para evitar contaminar la historia con otra cámara. Portarla
  separadamente sólo si se necesita esa compatibilidad; probar ausencia del mod,
  cámara principal y pases anidados. No añadirlo como nueva dependencia obligatoria.
- **Fallback de motion vectors:** su dispatcher permite usar el framebuffer del
  generador cuando falta una textura de shader. Puede ser útil si se comprueban
  resolución, escala, jitter y origen; no basta con una textura no nula.
- **Historia/copia real propia y slots por generado:** son decisiones útiles para
  un interpolador experimental. Nuestra implementación ya conserva outputs propios
  y leases para x2, por lo que no es necesario reemplazarla para obtener esa ventaja.
- **Cambio dinámico de algoritmo:** la funcionalidad es razonable; implementarla
  a través de contratos públicos y drenaje seguro, conservando capability checks.

`DlssgPipeline.cpp` es un stub con Dispatch vacío. Eso no demuestra que todos los
providers NGX/Streamline Java estén vacíos: el proyecto mantiene otros adapters.
No usar ese stub como evidencia de una integración nativa DLSS terminada.

## Artefactos revisados y límites de la revisión

Los dos JAR en `D:/ProjectDlssmiAmigo/build_jars` contienen clases propias major 65
(Java 21). Wisteria empaqueta automáticamente DLL Windows y SO Linux: no necesita
que el jugador seleccione manualmente esa biblioteca. No verifiqué ABI/exportaciones
mediante ejecución ni equivalencia exacta del binario con todas las fuentes.

- SR JAR `dev.79a5f53.opengl`, SHA256:
  `33c8334f3640735952bdf6a07262ec3ebca133861efccd80a7a49d171ae52d27`.
- Wisteria JAR, SHA256:
  `21233412951a6d063ed20f7f7906307b74eb4fbfcef2874236d8c42412252457`.
- DLL Wisteria empaquetada, SHA256:
  `1fc4fc2168f9bd25cd9f626aaaad9772038f9da14335314ac682ed9d29a63ec4`.

Inventario, diffs y metadatos: `D:/ProjectDllsss/logs/research/friend-comparison/`.
La comparación excluye dependencias vendorizadas/cachés y no es un inventario
exhaustivo de todos los binarios. Los defectos marcados por flujo de código son
estáticos; FPS, calidad, ausencia de crashes y funcionamiento x3–x6 permanecen
sin validación runtime en esta revisión.

## Orden recomendado

1. Conservar nuestra baseline oficial x2 y sus fixes.
2. Mejorar el proyecto del amigo primero en propagación de errores, capabilities,
   formatos y backpressure; adaptar APIs a su versión sin copiar archivos a ciegas.
3. Separar y nombrar correctamente su interpolador experimental, si interesa
   conservarlo. Validarlo primero en x2 con imágenes y métricas reales.
4. Sólo después evaluar sus fases múltiples como algoritmo propio y sus costes.
   Este paso no está implementado ni habilitado por esta revisión.
5. Incorporar a nuestro proyecto sólo mejoras seleccionadas de compatibilidad,
   con cambios y pruebas independientes; no reemplazar el bridge AMD validado.

Nuestros repos siguen limpios en SR `e98b3fdb907a84f9b9fc0d5f6f45f7846158ba4c`
y Wisteria `322ff7acc733c3699dd161a36f143584afcc157f`. Sin cambios de runtime,
sin push/PR, sin escritura en el proyecto del amigo ni en `.minecraft` real.
