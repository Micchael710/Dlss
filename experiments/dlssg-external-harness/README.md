# dlssg_external_harness

Harness C++20 Windows x64, un coordinador y worker aislado por ejecución.
NGX Vulkan o D3D12 público offscreen, sin Minecraft/JNI/PresentWorker/swapchain.
Fixture determinista, warmup4, count1/index1, fences y readback canónico.
Componente sdli0.3.5 fijado por SHA256, INI local y carga pública absoluta.
Sin ABI privada, cap overrides, OptiScaler ni descarga automática.

Build: tools/build_dlssg_external_harness.ps1, MSVC18/Ninja/Release /MT.
Runner: tools/run_dlssg_external_harness.ps1 -Api Vulkan|D3D12.
Variante B exige reviewed-result del gate Vulkan; no lanzamiento automático.
No repetir tests GPU de la fase cerrada: una prueba por variante, sin retries.
`--self-test DIRECTORY` prueba sólo CPU: veredicto/cámara y checkpoint bloqueado.
Reducer `tools/summarize_dlssg_external_harness.py RUN --review` escribe una
interpretación derivada y conserva originales.

DLL/lib/headers NGX y componente externo son dependencias locales identificadas
con licencias propias; ningún binario se incluye en Git. Un clon limpio necesita
esas dependencias legítimas para build/runtime.

Vulkan falló en kernel creation. D3D12 no llegó a Create por persistencia del
host; corrección posterior validada únicamente CPU. Ningún x2 NVIDIA PASS.
Detalles: docs/DLSSG_VULKAN_X2_RUN_REVIEW.md.
