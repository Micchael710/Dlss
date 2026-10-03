# Harness único: validación externa DLSS-G SM86

2026-10-03. Estado: DESIGN_READY; implementación/ejecución NO realizadas.
Auditoría estática cerrada. Sin nueva investigación de ABI privada.

## Alcance y estructura

Un solo ejecutable futuro, `dlssg_external_harness`, con coordinador y proceso
worker nuevo por escenario. Los workers no comparten módulos ni estado NGX.
Primera variante: NGX Vulkan offscreen. D3D12 offscreen es sólo una variante
reservada del mismo ejecutable, sin diseño detallado ni implementación ahora.
No Minecraft, mods, JNI, PresentWorker, superficies, swapchain ni presentación.
No x3–x6, OptiScaler completo, spoof global, DRS, cambios de driver/perfiles.

Componentes conceptuales: ExternalLoader, VulkanNgxSession, SyntheticFixture,
GpuReadback, EvidenceRecorder y VerdictReducer. ExternalLoader sólo conoce
contratos públicos. No resuelve, invoca ni deduce GetInfo/Install/SM86Bridge.
Los helpers oficiales NGX Vulkan y recursos offscreen sustituyen al juego.

El coordinador conserva evidencia y arranca el worker adaptado únicamente cuando
se autorice ejecución comunitaria. No se implementa un lanzamiento en esta fase.
El aislamiento de proceso evita contaminación de la baseline, pero no bloquea
por sí solo escrituras globales del módulo: la variante elegida debe tener
configuración y comportamiento sin escrituras de driver/perfiles. Si eso no puede
establecerse, no ejecutar esa variante; registrar PRECONDITION_BLOCKED.

## Cinco clases de evidencia, sin promoción implícita

| Clase | Evidencia válida | No basta |
|---|---|---|
| REPORTED_CAPABILITY | Getter real + código + valor + procedencia/hook desconocido. | Available=1, FeatureInitResult=1, max=5. |
| ORIGINAL_NGX_CAPABILITY | Proceso stock limpio, runtime identificado, sin DLL comunitario/OptiScaler. | Puntero resuelto en módulo NVIDIA después de instalar hooks. |
| ACTUAL_FEATURE_CREATION | CreateFeature Success, handle no nulo, comandos registrados/submitted y completion cuando corresponda. | Capability o install.active. |
| ACTUAL_KERNEL_CREATION | Evento backend kernel_create con status de creación exitoso, ruta/SM real, runtime/contexto y tiempo vinculados al worker. | Selección ptx_sm86, módulo cargado o inferencia desde Available. |
| ACTUAL_GENERATION | Evaluate x2 exitoso + submit/completion + readback válido atribuible al intervalo, G1 diferente de A/B y de sentinel. | Return Success, counter, VkImage handle o hash de memoria sin inicializar. |

La ausencia de logs kernel_create produce UNKNOWN/NOT_OBSERVED para esa clase,
no un éxito inventado. Una generación demostrada no permite fabricar los detalles
de creación de kernels que el componente no exponga.

## Stock separado y procedencia

Referencia actual, ya medida: `logs/runtime/ngx-stock-probe/20261003-141624/`.
Available=0; max=null, getter UnsupportedParameter; GPU0x170, mínimo0x190.
No repetir esta medición durante el diseño ni reconstruir su probe.

El futuro harness puede importar esa referencia si GPU/driver/runtime coinciden.
Si cambia el runtime elegido, la GPU/driver o se solicita reproducción, toma una
medición stock en un worker nuevo sin carga comunitaria y la cierra antes de
arrancar el adaptado. No intentar «volver a stock» descargando el DLL adaptado.
Mantener las mediciones de runtimes diferentes explícitamente diferenciadas.

`stock_original_available` y `stock_original_max` son valores stock, nunca
defaults. Conservar getter status, tipo solicitado y null si falla.
`adapted_reported_available`/`adapted_reported_max` proceden de la consulta del
worker adaptado; pueden estar intervenidos aun sin OptiScaler completo.
No denominarlos original. `adapted_original_* = null, provenance=NOT_OBSERVABLE`
si el contrato público no permite separar la respuesta anterior al hook.
No quitar hooks ni leer parámetros mediante ABI privada para obtenerla.

Identificar módulo realmente elegido por NGX, no sólo el candidato configurado:
paths absolutos, SHA256, versión, firma, commit/release de procedencia, módulos
cargados, callback NGX y manifest del bundle. Registrar también fallback/OTA.
Comparación stock/adaptado no es causalmente equivalente si cambió el runtime.

## Contrato público de ExternalLoader

1. Preparar un directorio de trabajo exclusivo del run. Módulo identificado y
   autorizado previamente; no descarga automática de binarios ni selección de
   archivos desconocidos. No cargar el pack DLSS-Unlocked completo.
2. Escribir `dlssg_sm86.ini` junto al módulo antes de LoadLibrary. Para la variante
   sdli conocida: Enabled=1, Mode=Bundled, Router=SM86, KernelImage=Auto,
   Optimized=0, MaxGeneratedFrames=1 y logging de kernels. Optimized=0 conserva
   sustitución SM86; no usar KernelImage=Original. El límite INI es intención de
   esta prueba x2, no capability original. SpoofArchToGame=0 para este host NGX
   directo sin gate de juego/Streamline; no modificar GPU física/minimum a mano.
3. Cargar ruta absoluta de `dlssg_sm86.dll` mediante carga Windows documentada;
   registrar resultado Win32, HMODULE, SHA y módulos posteriores. Establecer
   orden de carga de forma secuencial fuera de DllMain/loader lock.
4. sdli: inicialización del proxy mediante su carga, según contrato auditado.
   Otras variantes sólo si están identificadas y su loader público documenta
   InitializeASI o DLSSG_UniversalProxy con firma void(void); llamar únicamente
   el inicializador correspondiente. No explorar ni probar exports al azar.
   No control de perfiles/DRS ni Smooth Motion. No callbacks privados.
5. Guardar/seguir logs de instalación con timeout acotado y marcas de tiempo.
   LoadLibrary Success no es BACKEND_INSTALL_PASS. Si la instalación es asíncrona
   y no hay señal pública/log de readiness, registrar READINESS_UNKNOWN; no
   afirmar que una barrera de CPU prueba instalación.
6. Mantener módulo cargado hasta que termine el proceso, incluso tras destruir
   la feature. No FreeLibrary/hot unload de un módulo que instaló hooks.

Este harness reproduce el contrato esencial carga+INI. External=true y
AmpereMfgUnlock=true son flags de OptiScaler, no parámetros que deban enviarse
al DLL ni requisitos de cargar OptiScaler en el harness.

## Variante A: secuencia NGX Vulkan

1. Resolver precondiciones y escribir INI; cargar/inicializar módulo externo
   antes del primer init NGX. Registrar readiness y entorno, no dar PASS FG.
2. Crear VkInstance/physical device RTX3050Ti/VkDevice/queue(s) siguiendo los
   requisitos públicos del SDK fijado. UUID/LUID/driver, extensions/features
   requeridas/soportadas/activadas, queue family y validation messages en evidencia.
   Fallos de creación Vulkan son ENVIRONMENT, no fallo de generación SM86.
3. NGX Init con proyecto CUSTOM propio; callback a archivo. Capturar result code.
   GetCapabilityParameters y cuatro getters iguales al stock. No Set sobre las
   capabilities para desbloquear. Crear parameters de feature separados.
4. Crear fixture y outputs offscreen válidos, todos con identidad/format/extent/
   usage/layout. Recursos distintos de entrada/salida, no alias entre A, B y G1.
5. Un intento controlado de `NGX_VK_CREATE_DLSSG` con command buffer y parámetros
   oficiales. No depende de Available=1: la capability sólo es evidencia. Exigir
   Init Success y precondiciones válidas; no forzar la capability si es 0.
6. Guardar result+handle. Si registra trabajo GPU, submit/fence y completion
   antes de usar la feature. Capturar backend kernel_create por PID/contexto;
   puede ocurrir en Create o diferirse hasta Evaluate. No asignar fase por suposición.
7. Sembrar historia con A tras reset y warmup corto definido. No considerar el
   primer intervalo tras reset como prueba de interpolación. A/B son frames
   consecutivos de un mismo contexto/escena, no dos texturas arbitrarias.
8. En intervalo estable A→B, B es input actual y A la historia previa. Llamar
   `NGX_VK_EVALUATE_DLSSG`, count=1/index=1/reset=false, G1 preinicializado a sentinel.
   Mantener recursos/metadata inmutables y vivos hasta completion.
9. End command buffer → submit → wait fence/timeline acotado. Registrar Vulkan
   result, completion sequence, elapsed time y cualquier device lost. No bloquear
   indefinidamente. No sumar generación al retornar Evaluate sin completar GPU.
10. Barrier de output write→transfer read, copia a staging host visible, wait,
    invalidate para memoria no coherente. Canonicalizar sólo píxeles definidos,
    sin row padding. Readback y hashes de A/B/G1/sentinel, más imágenes inspeccionables.
11. Registrar output-disable-interpolation si se proporciona y su lectura GPU:
    buffer según contrato SDK, no interpretar un warmup/reset como generación.
    Si el runtime sugiere deshabilitar, no aceptar ese intervalo como x2 correcto.
12. ReleaseFeature, parameters, NGX Shutdown, GPU completada y recursos Vulkan
    destruidos fuera de DllMain. DLL externa queda cargada hasta process exit.

Fuente de llamadas: headers oficiales locales `nvsdk_ngx_helpers_dlssg_vk.h`
L47–63 (Create), L66–83 (Evaluate/recursos/count/index) y L192 (EvaluateFeature).
No invocar exports privados del snippet ni una vtable inventada.

## Fixture y control de output

Escena sintética determinista con cámara perspectiva fija, fondo texturado y
objeto texturado que se traslada entre A y B; t intermedio tiene posición conocida.
Depth, motion vectors y matrices se generan junto con esa escena y usan la
convención de dirección/escala del SDK. Cámara/matrices válidas y consistentes,
jitter cero real en la fixture, SDR y transferencia explícitos. Sin UI; HUDless
equivalente a escena si se proporciona. No inventar metadata de Minecraft.
FrameId/timestamps del host se registran como tales; no se asignan claves NGX
no expuestas por los headers públicos.

Comenzar a resolución moderada; declarar y comprobar formatos aceptados por
runtime y VkFormatProperties. Rechazo por formato/metadata invalida la fixture:
no atribuirlo automáticamente a SM86. El pipeline host verifica por readback
A/B/depth/MV antes de inferir un fallo del backend.

Sentinel es patrón determinista de alto contraste distinto de A/B, escrito por
el host antes del Evaluate con barrera. A/B/G1 tienen VkImages diferentes;
handles distintos no implican contenido distinto. Comparar SHA256 sobre pixeles
canónicos y comprobar que el resultado no conserva el sentinel, no es negro/
constante/corrupto y desplaza contenido de manera coherente con el intervalo.
Una diferencia de un píxel/debug overlay frente a A/B no prueba interpolación.
Inspección breve de las imágenes y métrica de movimiento de la fixture dan
evidencia adicional; no exigir igualdad bit a bit a una referencia de calidad AMD.

Warmup acotado (por ejemplo cuatro frames coherentes, configuración registrada).
Si nunca aparece un intervalo estable habilitado, clasificar INCONCLUSIVE_OUTPUT
con evidencia de reset/disable; no fingir PASS ni escalar automáticamente a D3D12.

## Artefactos y campos

Directorio exclusivo `logs/runtime/dlssg-external-harness/<run-id>/` futuro:
run-manifest.json, result.json, events.jsonl, ngx-callback.log, backend logs sin
alterar, vulkan-validation.log, fixture.json y readbacks/imágenes A/B/G1/sentinel.
Coordinador conserva logs parciales incluso si hay crash/timeout; PID y exit code.

Campos mínimos de result/event:

```text
scenario, api, runId, pid, gpu UUID/LUID, driver, DLL/runtime SHA256/version/path
stock_original_available, stock_original_max, stock_getter_result[each]
adapted_reported_available, adapted_reported_max, adapted_getter_result[each]
adapted_original_* = null si no observable; capability_provenance
loader_result, initializer_name/result, backend_install_observation
ngx_init_result, create_result, feature_handle
kernel_creation_status/evidence_ref, actual_sm/target_sm si log disponible
realFrameId A/B, intervalId, timestamp, deltaMs, reset, count=1, index=1
evaluate_result, generatedFrameId/slot (sólo validado tras readback)
VkImage/VkImageView/format/extent/layout, commandBuffer, queue, submit_id
completion_result/value/time, device_lost, output_disable_interpolation
readback_status, A/B/G1/sentinel hashes, changed-pixel/fixture-motion evidence
generated_count_confirmed, stage_result[each], if_fail_stage, cause_confidence
```

Counters host solicitados/submitted/completed/confirmed son distintos. Sólo el
último aumenta al demostrar generación; no usar un counter comunitario como único
testigo. Preservar codes hex y nombres de la versión SDK correcta.

## Veredicto y clasificación causal

DLSSG_SM86_VULKAN_X2=PASS exige Create success, Evaluate count1/index1 success,
completion GPU, G1 escrito y distinto de A/B, contenido interpolado válido y
sin device lost. Outputs no presentados: PresentWorker permanece intacto fuera
del harness. Readiness/capabilities/success de setters nunca producen ese PASS.

Mantener kernel_create OBSERVED/PASS sólo con logs exitosos; si faltan, UNKNOWN.
No cambiar UNKNOWN a PASS por generación ni afirmar un tipo de kernel no observado.
Si no se puede atribuir el output al Evaluate del intervalo, global INCONCLUSIVE.

| IF_FAIL_STAGE | Evidencia necesaria / distinción |
|---|---|
| architecture gate | Callback NGX muestra GPU/minimum y rechazo del snippet efectivo, no sólo Available0. |
| feature creation | Create falló/handle inválido sin causa más específica; code y log exactos. |
| kernel creation | Backend/driver registra creación de kernel fallida o imagen incompatible vinculada a Create/Evaluate. Sin log: causa desconocida. |
| evaluate | Feature válida, Evaluate falla; contar desde aquí sólo si no hay kernel error más específico. |
| synchronization | Submit/wait falla, timeout o device lost; registrar API exacta, no inventar causa kernel. |
| output | GPU completó, pero sentinel/duplicate/black/corrupt/readback inválido o output no atribuible. |

Precondiciones/loader/init/fixture poseen errores propios antes de esa lista.
No clasificar una firma/INI/ruta/formato incorrectos como bloqueo SM86 Vulkan.
Registrar tanto etapa observada como causa, que puede ser UNKNOWN. Device lost
detiene ese worker; no iniciar otra prueba GPU automáticamente tras driver reset.

## Condición estricta para variante B

No diseñar ahora NGX D3D12 offscreen ni construir sidecar.
Primero ejecutar A en una fase posterior autorizada. B sólo se habilita para
diseño si A no demuestra creación/generación y evidencia concreta vincula el
bloqueo a la ruta SM86 Vulkan, después de excluir errores del harness/loader.
Un Available0, crash sin causa o timeout no justifican por sí solos la transición.
Si A pasa: NEED_D3D12_SIDECAR=NO. Si A está sin ejecutar/inconcluso: NO (no activado;
necesidad futura indeterminada). Si queda probado el bloqueo de ruta: YES para
evaluar la alternativa; el harness B primero debe probar D3D12 offscreen sin
swapchain. Sólo después se justificaría diseñar transporte sidecar.

## Estado al cerrar este diseño

EXTERNAL_LOADER_READY=DESIGN_READY, runtime NOT_RUN.
VULKAN_CREATEFEATURE/KERNEL_CREATE/EVALUATE/OUTPUT=NOT_RUN.
DLSSG_SM86_VULKAN_X2=NOT_RUN; IF_FAIL_STAGE=NONE_NOT_RUN.
NEED_D3D12_SIDECAR=NO, activación condicional pendiente de evidencia A.
Sin nuevos experimentos, compilación, descarga/ejecución comunitaria, cambios de
driver/perfiles, Minecraft, push/PR o modificación de AMD FSR FG x2.
