# Medición real NGX DLSS-G Vulkan, RTX 3050 Ti

2026-10-03. PROBE_STOCK_CAPABILITY_QUERY=PASS. Resultado FG stock: no disponible.
Esto es una medición válida, no un fallo del probe ni de la baseline AMD.

## Entorno y procedencia

- GPU seleccionada: NVIDIA GeForce RTX 3050 Ti Laptop GPU, vendor NVIDIA.
- Vulkan device UUID: 01895b66d1ca454d88788dd21fdef638.
- LUID válido: 4c29010000000000; es identidad de esta sesión, no promesa permanente.
- Driver: 596.49, reportado por VkPhysicalDeviceDriverProperties y driverVersion.
- NGX core API 0x15; motor CUSTOM, UUID propio del probe; sin AppID de juegos NVIDIA.
- Runtime stock: nvngx_dlssg.dll 310.9.1.0, firma NVIDIA Valid.
- SHA256: ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82.
- Origen: NVIDIA/DLSS commit 374959484e79a640feaba44c93ac8cfb0a03f5b5,
  lib/Windows_x86_64/rel/nvngx_dlssg.dll, sin modificar.
- SDK import/static lib: nvsdk_ngx_s.lib del mismo commit, SHA256
  4e5d355086d2bc11e1a0842457d2519ea528ee1f3e112c45679a84960c07dff3.

Fuentes, probe y dependencias quedan en experiments/ngx-dlssg-probe. No se copiaron
al mod ni a Minecraft. Sólo se construyó este ejecutable; no se reconstruyeron
componentes ya aprobados. DLL/lib oficiales excluidos de versionado, licencia conservada.

## Resultado medido

| Llamada / campo | Result code | Valor |
|---|---|---|
| NGX_VULKAN_Init_with_ProjectID | 0x00000001 Success | inicializado |
| NGX_VULKAN_GetCapabilityParameters | 0x00000001 Success | mapa disponible |
| FrameGeneration.Available, GetUI | 0x00000001 Success | **0** |
| FrameGeneration.FeatureInitResult, GetI | 0x00000001 Success | **3134193675 = 0xBAD0000B** |
| FrameGeneration.NeedsUpdatedDriver, GetUI | 0x00000001 Success | **0** |
| DLSSG.MultiFrameCountMax, GetUI | **0xBAD00010 UnsupportedParameter** | **null / no reportado** |
| DestroyParameters / Shutdown1 | 0x00000001 Success | limpieza terminada |

En estos headers, 0xBAD0000B significa **FAIL_UnableToInitializeFeature**, no
FAIL_OutOfDate (que es 0xBAD0000C). Se conserva el código original y el getter
result por separado: leer correctamente FeatureInitResult no significa que el feature pase.
No adoptar automáticamente los nombres de error atribuidos por reportes comunitarios.

CreateFeature=SKIPPED_FG_UNAVAILABLE. Evaluate no existe en el código del probe.
Generated frames=0. Ningún multiplicador NVIDIA es PASS.

No hay max numérico leído. **No convertir UnsupportedParameter a cero, uno o cinco.**
El fallback semántico x2 de un max ausente sólo aplica si FG está disponible;
en este sistema FG=0, por lo que ni x2 se habilita. MAX_MULTIPLIER_DERIVED=no disponible.
Cuando haya un max válido, max+1 será sólo una interpretación aritmética de capability,
no validación de generación/presentación.

## Bloqueo concreto stock

El callback oficial indica para la DLL local verificada:

- GPU architecture 0x170; snippet mínimo 0x190.
- Rechazo explícito por arquitectura más nueva requerida.
- El log identifica el snippet local y versión 310.9.1.
- El fallback del DriverStore también se intenta y rechaza por la misma comparación.

La enumeración de módulos confirma el DLL stock local, NGX core del driver y NVAPI
del sistema, sin proxy comunitario. Driver/cache son búsquedas normales del SDK;
no se editaron caches, perfiles ni configuraciones globales. Warnings de otros
features NGX no instalados no cambian el resultado de los cuatro getters FG.

## Escenario adaptado

SM86_AVAILABLE=NOT_RUN; SM86_MAX=NOT_QUERIED. No ruta Vulkan/backend adaptado con
ABI reproducible disponible. Ninguna carga ni invocación de la DLL comunitaria.

result.json diferencia la comparación entre procesos stock/adaptado de observaciones
original_ngx_*_before_hook y reported_*_after_hook dentro del escenario adaptado.
Estas últimas son null y no se fabrican a partir de Available=0 del proceso stock.
El futuro escenario B deberá repetir las mismas llamadas, identificar ABI/backend,
hash/version/licencia y registrar por separado la consulta antes/después del hook.

Siguiente gate condicional: sólo Available=1 permitirá CreateFeature. Sólo contexto
creado y permisos/ruta reproducible permitirán diseñar/probar x2 count=1/index=1.
No generación en esta fase. x3..x6 requieren éxito real secuencial, no capability sola.

## Evidencia y verificación

Directorio de medición: D:/ProjectDllsss/logs/runtime/ngx-stock-probe/20261003-141624/.

- raw-result.json: salida directa del C++ con valores/getter codes/identidad GPU.
- runtime-identity.json: hash, firma, versión y commit del candidato stock.
- ngx-callback.log: carga/rechazo del snippet, arquitectura y limpieza.
- result.json: agregado validado; separación stock/adaptado/hook.
- driver-fallback.json: identidad/hash del fallback observado, leído sin modificar.
- console.log: stdout original y salida normal del ejecutable.

summarize_ngx_stock_probe.py verificó el runtime observado por su ruta/hash,
el dispositivo elegido, init/query exitosos, ausencia de Evaluate, getter fallido
preservado como null y Create no ejecutado con Available=0.
Compilación MSVC Windows x64 terminada. La comprobación CMake del compilador se
bloqueó dentro del sandbox; el build aislado aprobado fuera de él completó correctamente.
La consulta GPU después se ejecutó con permisos normales.

Hashes baseline SR/Wisteria/bridge AMD coinciden con los conservados:
20fa05a42731692d5c20aa2cd82d7ff1367febc4fc1261fcad1a3a3c478c0896;
ac16837b0447d14f4cdff4ed00c75bb84feb2a80a87d261f690b47914f5a8e53;
b0388beb885271fafa59e788bb4f9c2ce7d508ae4ecb11879b113a7f0d043f7b.
AMD_FSR_FG_X2=PASS conservado. PresentWorker, ring fix, interop y OptiScaler OFF
intactos. No imágenes GPU ni swapchain/present en el probe; no .minecraft real,
push o PR. No repetición de gates AMD/Minecraft aprobados.
