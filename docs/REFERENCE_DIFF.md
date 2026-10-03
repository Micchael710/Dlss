# Comparación dirigida — 2026-10-02

Estado: análisis estático terminado para los puntos de integración solicitados; no prueba de ejecución ni auditoría completa de los binarios. Bases actuales: SR `e365f5b32df88be8fb9925aaf0491658ebf4f705`, Wisteria `aeed1ef78e23ef0f8de51ad65d8804af17457f61`. Los JAR antiguos permanecen read-only en Apparatia Alpha; hashes en REFERENCE_INVENTORY.md y logs/design/reference-static-inventory.json. La versión anterior de SR es 0.9.1-alpha.2; la baseline actual 0.9.2-alpha.1: una diferencia no puede atribuirse automáticamente a nuestra modificación anterior sin su fuente/historial.

Evidencia dirigida: logs/design/sr-reference-javap.txt, sr-reference-scheduler-javap.txt y wisteria-reference-javap.txt. Se inspeccionaron registry/provider/request, scheduler antiguo y cinco clases Wisteria relacionadas con FSR; no se descompiló todo el mod. Rutas actuales en FRAME_GENERATION_ARCHITECTURE.md.

## Super Resolution

| Diferencia / clasificación | Qué resolvía | Aplicación actual y equivalente | Concepto reutilizable / riesgo |
|---|---|---|---|
| API en api.registry, AsyncFrameGenerationDispatchRequest/Result, ProviderOutputLease — OBSOLETA | Contrato de plugins y salidas async | api.registry.framegeneration; FrameGenerationDispatchInput/Result y FrameGenerationProviderOutput | Adaptar la intención; importar clases antiguas rompe compatibilidad binaria y contratos de lifetime |
| Registro de providers y grupos — ÚTIL | Selección de algoritmo, implementación y prioridad | FrameGenerationRegistry/Description, BackendNegotiator; duplicados rechazados | Registro por evento y opciones. Prioridad efectiva descendente; no asumir selección por orden de inserción |
| AsyncFrameGenerationScheduler — OBSOLETA | Snapshots, colas, submission, retirement y pacing | AsyncFramePresenter + FrameGenerationWorker + PresentWorker | El javap confirma submitApplicationManagedFrame, releasePendingProviderLeases y sleepUntil. No transplantar worker viejo ni duplicar pacing |
| Snapshot immutable y frame index — ÚTIL | Evitar lectura de cámara/configuración mutable desde worker | ProviderInputSnapshot y validación ID/índice antes de dispatch | Mantener capture en render thread. Aún faltan campos AMD; no usar getters de render globals en FG thread |
| Captura y Vulkan interop — ÚTIL | Proporcionar entradas Vulkan desde GL | FrameResources, CaptureFrameRing, FrameTextureResource, VkGlInteropSemaphore | Mantener ready/release y tres frames en vuelo; un handle no constituye ownership |
| Configuración custom de backend/multiplicador — POSIBLEMENTE ÚTIL | Exponer selección y número de frames | SpecialConfigDescription, FrameGenerationMode y capabilities actuales | Exponer solo Vulkan y x2 inicialmente. No interpretar capacidad del scheduler (hasta cinco) como soporte AMD |
| Scheduler/presentación propios del JAR — NO NECESARIA | Llevar salida custom al display | PresentWorker ya posee acquire/blit/present y pacing | Evitar dos dueños de swapchain. Compatibilidad de DLL AMD directa debe probarse antes de decidir cambios |

## Wisteria

| Diferencia / clasificación | Qué resolvía | Aplicación actual y equivalente | Concepto reutilizable / riesgo |
|---|---|---|---|
| wisteria:fsr, grupo wisteria:fsr_fg y prioridad 150 — ÚTIL | Registrar algoritmo distinto de DLSS | Hoy solo wisteria:ngx (200) / wisteria:streamline (100), grupo DLSS_FG | No hay conflicto estático en checkout actual; verificar runtime al registrar. Grupo custom es necesario: FrameGenerationGroups actual solo define DLSS_FG |
| FsrFrameGenerationBackend async — ÚTIL | Delegar lifecycle/dispatch | NgxFrameGenerationBackend es patrón actual | Reutilizar estructura conceptual, excluyendo NVNGX y antigua VulkanAsyncFoundationBridge |
| OutputSlot/SlotKey/SlotLease — ÚTIL | Evitar reutilizar imágenes aún presentadas | NgxFrameGenerationAdapter y FrameGenerationProviderOutput | Crear pool propio por sesión y drenar leases antes de recreate; no copiar los tipos antiguos |
| LayoutTransition GENERAL/restauración — POSIBLEMENTE ÚTIL | Preparar sampling/storage | Adapter actual y tracker VulkanTexture | GENERAL no sustituye barriers, access masks ni queue-family transfers; sincronizar estado real y tracker |
| FsrFgBackend AUTO/VULKAN/D3D12 — NO NECESARIA | Selección de backend nativo | Plan inicial únicamente Vulkan Windows x64 | isAvailable devuelve true para AUTO/VULKAN en el enum antiguo: no demuestra capacidad GPU ni pipeline oficial |
| FsrNativeBridge globals setAlgorithm/setMultiplier/setFsrBackend — OBSOLETA | Selección global del pipeline custom | Nuevo contrato propuesto de sesión opaca, ABI versionada | Evitar estado nativo global compartido y raw pointers sin generación |
| dispatchFrameFsr(JJJJJJJJIIFFZ) — DEFECTUOSA como contrato AMD completo | Generar interpolados con handles y pocos parámetros | FFX prepare/configure/dispatch con descripciones oficiales | No transporta la información AMD completa de cámara, color, tiempos y ownership. Esto no demuestra por sí solo la causa de crashes anteriores |
| WisteriaNative.dll y SPIR-V custom — NO NECESARIA | Pipeline Vulkan propio | DLL AMD Vulkan firmada + bridge nuevo futuro | No cargar ni redistribuir ni tomar como FG oficial. Referencia únicamente |
| Multiplicadores altos de referencia — NO NECESARIA | Generar varios outputs por real | Propuesta un generado por real, x2 total | Arrays de outputs en ABI no prueban múltiples interpolados soportados por el efecto elegido |

La DLL leída sin ejecutarla tiene 30,720 bytes, SHA256 `1c586aaae66ce07b3f1309bcf5fa0a8e9e46e92a3393ed92bca70b4f6c407340`; contiene `Fsr3Pipeline`, `interpolatedMotion`, SPIR-V/GLSL.std.450 y creación de shader, descriptors y compute pipeline Vulkan propios. Los exports encontrados son cuatro funciones FsrNativeBridge. La ausencia de strings ffx/OpticalFlow no prueba por sí sola ausencia de código estático; no se pretende haber reconstruido toda la matemática. La caracterización previa de shader sintético procede de la solicitud del usuario; esta inspección corrobora el pipeline custom, no reproduce sus crashes ni valida el informe anterior.

## Shader y evidencia no disponible

Complementary permanece intacto. Los bindings de referencia son color autotex3, depth depthtex, MV autotex2; SR BEFORE composite7. La reproyección de cámara/profundidad no entrega por sí sola movimiento completo de entidades. Jitter, sky, DH y formatos requieren validación visual posterior. El nombre del buffer no acredita unidades o transfer function.

No se localizó MINECRAFT_RENDER_AUDIT.md ni crash reports de referencia en el workspace/material adjunto identificado. No se afirma haberlos leído. Los logs locales existentes documentan builds, no crashes GPU. Quedan pendientes la fuente del backend antiguo y evidencia runtime; su ausencia no impide este diseño, sí impide atribuir una causa exacta a esos crashes.
