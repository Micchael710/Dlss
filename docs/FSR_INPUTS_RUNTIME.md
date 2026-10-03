# Inputs del primer x2 — 2026-10-02

| Input | Estado | Clasificación | Fuente y límite |
|---|---|---|---|
| final color | READY | REQUIRED_FOR_FIRST_X2 | Captura RGBA8, tamaño display, VkImage compartida |
| HUDless color | READY | REQUIRED_FOR_FIRST_X2 | Captura antes del HUD, RGBA8, tamaño display |
| depth | READY | REQUIRED_FOR_FIRST_X2 | Depth24 convertido a R32F; profundidad no lineal normal, near/far reales |
| motion vectors | PARTIAL | REQUIRED_FOR_FIRST_X2 | RGBA16F, resolución render, reproyección cámara; UV convertida a píxeles en prepare, Y coherente con captura Vulkan |
| movimiento objetos/deformaciones | PARTIAL | QUALITY_LATER | No se inventan velocidades ausentes; manos y algunos objetos carecen de vector propio |
| jitter | READY | REQUIRED_FOR_FIRST_X2 | DispatchResource, unidades subpíxel del render real |
| camera matrices/basis | READY | REQUIRED_FOR_FIRST_X2 | FrameGenerationConstants copia arrays y posición/base Minecraft; CameraInfo SDK 3.1.4 |
| render/display size | READY | REQUIRED_FOR_FIRST_X2 | Records inmutables por frame; verificación contra imágenes y swapchain |
| timing/frame ID | READY | REQUIRED_FOR_FIRST_X2 | System.nanoTime medido por frame real, delta ms, ID monotónico; no tick delta |
| reset/discontinuity | READY | REQUIRED_FOR_FIRST_X2 | Epoch, cambio de geometría, reset cámara, discontinuidad ID |
| transfer | READY | REQUIRED_FOR_FIRST_X2 | Declaración IrisVideoSettings.colorSpace; UNKNOWN rechazado, no inferencia de RGBA8 |
| HDR luminancia | MISSING | QUALITY_LATER | No proporcionada; primer camino SDR SRGB, campos HDR sin usar |

viewSpaceToMetersFactor=1 procede de las unidades de bloques Minecraft y cámara
vanilla sin escala en view. No se anuncia soporte de cámaras externas con escala.
El SDK permite NO_SWAPCHAIN_CONTEXT_NOTIFY: prepare/dispatch graban en command buffer
del worker; el bridge no adquiere ni presenta swapchain. Outputs 0..5 por lease;
esta ruta anuncia y ejecuta únicamente un output, multiplier x2.
