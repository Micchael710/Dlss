# Auditoría del gate NGX y backend SM86

Fecha: 2026-10-03. Investigación estática; no instalación ni ejecución comunitaria.
AMD FSR FG x2, PresentWorker, ring e interop permanecen intactos.

Continuación: [DLSSG_EXTERNAL_HOST_AUDIT.md](DLSSG_EXTERNAL_HOST_AUDIT.md) localiza
una alternativa pública de carga+INI sin invocar esta ABI privada. Reverse engineering
directo de GetInfo/Install/SM86Bridge queda pausado. Compatibilidad Vulkan pendiente.

## Conclusión

El proyecto documenta una adaptación compuesta: respuesta de arquitectura para
el juego, mínimo del snippet corregido para NGX y sustitución de kernels por
imágenes compatibles con la GPU física. Cambiar únicamente capabilities o
simular Ada globalmente no reproduce esa adaptación. La implementación privada
que conecta esos cambios no está publicada: su reproducción en Vulkan es UNKNOWN.

## Evidencia y límites

- MEDIDO LOCAL: [callback stock](../logs/runtime/ngx-stock-probe/20261003-141624/ngx-callback.log),
  líneas 309–311 y 329–331: GPU 0x170, mínimo del snippet 0x190; rechazo en
  `NGXValidateSnippetMetaData`, antes de una generación. Available=0.
- DOCUMENTADO POR AUTOR: revisión sdli `9621db573e07ed54f50c15bbb585ed9a7bdfac28`.
  No equivale a una prueba del backend comunitario en nuestra RTX 3050 Ti.
- VERIFICADO ESTÁTICAMENTE: [metadatos PE conservados](../logs/research/dlssg-ampere/deep/sm86-component.json).
  Se conocen exports y hashes; no firmas C, hooks internos ni cobertura Vulkan.
- NO DEMOSTRADO: orden interno de llamadas Install, parche concreto del mínimo,
  compatibilidad de todos los kernels y ejecución Vulkan adaptada.

## Cuatro capas distintas

| Capa | Cambio documentado | Qué no demuestra |
|---|---|---|
| GAME/STREAMLINE ARCH SPOOF | `NvAPI_GPU_GetArchInfo` devuelve otra arquitectura sólo a determinados llamadores externos; por defecto Blackwell 0x1b0. | No convierte SM86 en SM89 ni resuelve por sí solo la comparación de metadatos NGX. |
| NGX CORE MINIMUM-ARCH GATE | El backend modifica la respuesta del mínimo exportado por el snippet, `NVSDK_NGX_GetGPUArchitecture`, para anunciar la arquitectura física. | No prueba que los kernels puedan crearse o ejecutarse. |
| SM86 BACKEND INSTALL | Backend seleccionado por SHA256 exacto del runtime, negociación ABI y selección de ruta según GPU física. | `install.active=true` y status=0 no prueban Evaluate. |
| KERNEL ARCHITECTURE | Creación de kernels sustituidos SM86: cubin correspondiente a SM86 o PTX de esa familia compilado por el driver. | Un log de selección `ptx_sm86` no prueba creación ni generación correcta. |

### GAME/STREAMLINE ARCH SPOOF

[INSTALL.en.md L157–158](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/docs/INSTALL.en.md#L157-L158)
describe redirección de la entrada EAT de `nvapi_QueryInterface` hacia un
trampolín y filtrado por módulo llamador. Se arma temprano porque Streamline
puede descartar el plugin en `slInit`, antes de cargar el snippet.

El core `_nvngx.dll`, snippets NVIDIA, componentes del driver y backend propio
deben seguir obteniendo la arquitectura real. El changelog 0.3.4 documenta
device hangs cuando el spoof alcanzaba modelos NVIDIA SR cargados desde el
model store. Esta política NO es un spoof global 0x170 → 0x190.

### NGX CORE MINIMUM-ARCH GATE

[INSTALL.en.md L260](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/docs/INSTALL.en.md#L260)
identifica dos operandos: arquitectura física guardada por el core durante
inicialización y mínimo declarado por `NVSDK_NGX_GetGPUArchitecture` del snippet.
El símbolo está en el PE del runtime; no es un getter público de capabilities.

El autor indica que el backend anuncia la arquitectura física mediante ese
export. Su ejemplo explícito es `turing_host.gpu_architecture_export=0x160`.
Para Ampere, el valor coherente con esa regla es 0x170: **deducción del contrato
documentado, no lectura de un backend adaptado en nuestra máquina**. No se
encontró una declaración pública de un campo equivalente `ampere_host` ni una
firma de la función que permita reproducirlo. No atribuirle un nombre inventado.

Esto corrige el lado mínimo del snippet, conservando GPU=0x170; no parchea
Available ni exige que el core crea que la GPU física es Ada. La documentación
también habla de un host interno Blackwell para el runtime (L158); eso es otra
adaptación interna, no evidencia de que NGX/NVAPI global reciban Blackwell.
No se conoce la estructura ni el mecanismo de ese host.

Los hooks `fg_gate_*` de los cuatro exports públicos del core se describen como
observadores (L252–260). No hay evidencia de que sean el parche del mínimo.
No se puede afirmar si el mínimo se cambia mediante detour, EAT, wrapper o
resolución de símbolos: faltan las fuentes.

Corrección de un error documental del autor: en los headers oficiales locales
`nvsdk_ngx_defs.h` L150/154, 0xBAD0000B = UnableToInitializeFeature;
OutOfDate = 0xBAD0000C. El texto comunitario usa el nombre incorrecto para 0xB.

### SM86 BACKEND INSTALL: traza disponible

1. El proxy activo intercepta carga de DLL y selecciona el runtime/backend.
2. Verifica correspondencia con el SHA256 del runtime, no sólo su versión.
3. Requiere `DlssgMod_GetInfo` y `DlssgMod_Install`; rechaza ABI/hash incompatibles
   ([INSTALL.md L242, versión china](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/docs/INSTALL.md#L242)).
   `DlssgMod_SupportedRouteFlags` es una extensión opcional (INSTALL.en L185).
4. El backend contiene `SM86Bridge_Install`, `SM86Bridge_ProbeRequirements`,
   `SM86Bridge_GetStats`, `SM86Bridge_SetObserver`, `DlssgMod_TuringHost`.
5. Según documentación, instala adaptación del host/mínimo y ruta de kernels.
   **No es posible trazar la llamada real DlssgMod_Install → SM86Bridge_Install,
   su orden ni parámetros a partir de una tabla de exports.**

El proxy exterior sólo exporta funciones de version.dll y DlssgProxy_Name/Role;
los símbolos de instalación están en el backend embebido, no en el proxy.
No se cargó ni extrajo ese backend durante esta auditoría.

Identidad estática ya conservada:

| Componente | SHA256 |
|---|---|
| Proxy 0.3.5 | c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838 |
| Runtime embebido 310.9.1 | ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82 |
| Backend embebido | 1aebf8f306fbaede18c98b372da9c70a1bbdb1bd35327f7f85c2969085a619a1 |

El runtime embebido coincide bit por bit con el DLL oficial del probe stock.
Por tanto, no se trata de un archivo NVIDIA recompilado con otro mínimo.
La intervención al cargar/ejecutar es una inferencia consistente con los hooks
documentados; el método exacto de modificación en memoria no está demostrado.

### KERNEL REQUIREMENT

INSTALL L76, L153–155 y L187–203 distingue `Optimized=0` de `KernelImage=Original`:
el primero conserva sustitución compatible; el segundo deja todos los kernels
originales. El autor indica que 310.9.1 incluye kernels de imagen sm_89 que
no pueden crearse en Ampere. Pasar el gate sin reemplazarlos no es suficiente.

La ruta necesita código máquina SM86 compatible o PTX adecuado, mapping de
kernels del runtime exacto, entry points, argumentos, texturas/buffers y lifetime.
El autor documenta selección cubin/PTX y fallback de cubin rechazado al PTX del
mismo kernel; no una reinterpretación del cubin SM89. Los cambios de runtime
requieren nuevos mappings (310.9 añade kernels/patches con RVAs específicos).
README L11 registra un bug de lookup al recrear la feature corregido en 0.3.5.

La superficie D3D12 está documentada, incluida `NvAPI_D3D12_LaunchCuKernelChain`
experimental (L161); no demuestra cuál es la llamada base utilizada por cada
kernel ni que el backend intercepte los equivalentes Vulkan.

## Vulkan: lo que falta exactamente

El runtime tiene exports Vulkan y nuestro probe stock inicializa NGX Vulkan.
Eso no prueba que el backend SM86 intervenga esa ruta. README L41/L45 delimita
el soporte principal a D3D12. El fork MFG en revisión
`cd4b4bb9f483ee3c59d3a912cd4075d1c8ebc742` declara en VULKAN_SUPPORT.md L3–16,
L38–44 y L65–91 que su ruta delega a un FeatureAdapter pendiente; no proporciona
la adaptación SM86 de NGX/kernels Vulkan.

No falta el *nombre* de un export. Falta el contrato reproducible:

- Prototipos y calling convention de `DlssgMod_GetInfo`, `DlssgMod_Install`,
  `SM86Bridge_Install`, `SM86Bridge_ProbeRequirements` y `DlssgMod_TuringHost`.
- Versión ABI actual, layouts/tamaños de structs, códigos de retorno, opciones
  y enlace entre GetInfo/Install/runtime HMODULE/hash. La mención histórica a
  ABI 1 no determina el contrato del binario 0.3.5.
- Función/contrato concreto que interviene `NVSDK_NGX_GetGPUArchitecture` antes
  de la validación, mantiene el host correcto y desinstala/restaura sus hooks.
- Instalación de kernels en la ruta Vulkan, mappings y blobs SM86 auditables,
  parámetros y cobertura de creación/launch. Los posibles puntos NVX/NV de
  CUDA-kernel Vulkan no están demostrados como utilizados por este backend.
- Ownership de VkInstance/device/queue/command buffer, recursos y sincronización,
  reinstalación por recreación de feature/device y cleanup. PresentWorker debe
  continuar presentando exclusivamente; el backend no debe poseer swapchain.

[Respuesta del autor sobre fuentes](https://github.com/sdli1995/dlssg_for_sm86/issues/524#issuecomment-5700639074):
fuentes no disponibles por dudas sobre uso de material NVIDIA. Main no contiene
los headers/cpp citados por sus documentos; init tampoco expone implementación.
README atribuye GPLv3 al código propio, pero kernels/runtime no se relicencian.
No empaquetar ni suponer permisos de redistribución para esos recursos.

Siguiente paso: obtener fuentes o headers ABI versionados y evidencia específica
de instalación/ejecución SM86 Vulkan. Hasta entonces no invocar Install con una
firma adivinada, parchear el mínimo aisladamente ni declarar adaptado PASS.
