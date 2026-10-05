# OptiScaler Source Baseline

Repositorio oficial: https://github.com/optiscaler/OptiScaler.git. Rama `master`; commit `97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed`; inspección 2026-10-04, America/La_Paz. Clon superficial en `D:/ProjectDllsss-research/OptiScaler`, sin modificaciones y sin inicializar submódulos, compilar ni ejecutar. El árbol externo se comprobó limpio.

Proyecto principal: HEAD inicial `bf44cf26ea5b87da4a8efd75d94f275030ffa3d1`, main limpio y SHA remoto confirmado. Streamline 2.12 permanece cerrado: EVALUATE_NOT_INVOKED, no Evaluate failed. Se aceptan los resultados históricos proporcionados sin repetir pruebas.

Las referencias siguientes están fijadas al commit inspeccionado. Son evidencia estática del enrutamiento, no una prueba de ejecución NVIDIA MFG en SM86. Los headers compilados por OptiScaler declaran SDK **2.11.1**, mientras las DLL se cargan de una carpeta externa `streamline`: no se demuestra una versión binaria concreta ni coherencia del conjunto sólo con este checkout. [external/streamline/sl_version.h:24-35](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/external/streamline/sl_version.h#L24-L35); [OptiScaler/OptiScaler.vcxproj:115](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/OptiScaler.vcxproj#L115); [OptiScaler/proxies/Streamline_Proxy.h:67-99](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/proxies/Streamline_Proxy.h#L67-L99).

# FG Input Architecture

`FGInput` y `FGOutput` son enums independientes; `FGInput::DLSSG` significa entrada Streamline, no identifica el algoritmo final. `FGInput::Upscaler` permite obtener datos de upscaling; también existen entradas NvngxFG, FSRFG, FSRFG30 y XeFG. Config lee ambos en `[FrameGen]`. [OptiScaler/State.h:31-75](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/State.h#L31-L75); [OptiScaler/Config.cpp:72-73](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/Config.cpp#L72-L73).

Adaptadores principales: `inputs/FG/Streamline_Inputs_Dx12.cpp`, `Upscaler_Inputs_Dx12.cpp`, `FfxApi_Dx12_FG.cpp`, `FSR3_Dx12_FG.cpp`. La entrada Streamline transfiere cámara, jitter, MV scale y recursos a `State::currentFG` mediante setters; la entrada Upscaler obtiene cámara del contrato de upscaling. [OptiScaler/inputs/FG/Streamline_Inputs_Dx12.cpp:224-242](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/inputs/FG/Streamline_Inputs_Dx12.cpp#L224-L242); [OptiScaler/inputs/FG/Streamline_Inputs_Dx12.cpp:289-432](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/inputs/FG/Streamline_Inputs_Dx12.cpp#L289-L432); [OptiScaler/inputs/FG/Upscaler_Inputs_Dx12.cpp:29-65](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/inputs/FG/Upscaler_Inputs_Dx12.cpp#L29-L65).

La interfaz normalizada es `IFGFeature`: constantes, slots de frames, readiness, validez y tipos Depth/Velocity/HudlessColor/UIColor/Distortion/Color. `IFGFeature_Dx12` añade recursos D3D12, estado, extent, lista y copias GPU. [OptiScaler/framegen/IFGFeature.h:8-115](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/IFGFeature.h#L8-L115); [OptiScaler/framegen/IFGFeature_Dx12.h:15-41](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/IFGFeature_Dx12.h#L15-L41).

# FG Output Architecture

`FGOutput` selecciona NoFG, FSRFG, DLSSG, XeFG o Reprojection. `FGHooks::CreateSwapChainForHwnd` instancia `DLSSG_Dx12`, `FSRFG_Dx12`, `XeFG_Dx12` o el backend de reproyección según output; no usa el nombre del input para decidir el generador. [OptiScaler/State.h:55-75](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/State.h#L55-L75); [OptiScaler/hooks/FG_Hooks.cpp:237-340](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/FG_Hooks.cpp#L237-L340).

Hay otra selección ortogonal, `FGNvngxReplacement`: None/Nukems/Arturs/FFX/Combo. Por ello **output DLSSG tampoco garantiza DLL NVIDIA genuina** si se habilitó un replacement. `Nvngx_FG::getProvider` crea el proveedor y puede hacer fallback entre proveedores. [OptiScaler/State.h:77-84](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/State.h#L77-L84); [OptiScaler/framegen/nvngx/Nvngx_FG.cpp:17-126](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_FG.cpp#L17-L126).

# NVIDIA DLSS-G Output

Implementación encontrada: `framegen/dlssg/DLSSG_Dx12.h/.cpp`, clase `DLSSG_Dx12 : IFGFeature_Dx12`, más `proxies/Streamline_Proxy.h`. Crea swapchain mediante factory actualizada, carga kFeatureDLSS_G, consulta max y guarda swapchain/queue; no implementa un scheduler NGX NVIDIA independiente. [OptiScaler/framegen/dlssg/DLSSG_Dx12.h:5-73](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.h#L5-L73); [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:100-170](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L100-L170).

`Dispatch()` prepara opciones, constantes y tokens; `Present()` devuelve `Dispatch()`. No equivale a una llamada explícita de OptiScaler a Evaluate NVIDIA. El trabajo interno y la generación son responsabilidad del plugin/runtime elegido por Streamline. [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:267-308](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L267-L308); [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:454-482](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L454-L482); [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:785-829](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L785-L829).

# InterpolationCount Flow

Cadena propia exacta:

`[DLSSG].InterpolationCount` → `Config::FGDLSSGInterpolationCount` → clamp a `_maxInterpolationCount` obtenido de `slDLSSGGetState.numFramesToGenerateMax` → `_framesToInterpolate` → `sl::DLSSGOptions.numFramesToGenerate` → `StreamlineProxy::DLSSGSetOptions(viewport, options)` → plugin `sl.dlss_g` → runtime NGX seleccionado.

Fuentes: parser acepta 1..6 [OptiScaler/Config.cpp:197-207](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/Config.cpp#L197-L207), valor default 1 [OptiScaler/Config.h:629-632](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/Config.h#L629-L632), límite runtime [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:155-163](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L155-L163), asignación y llamada final [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:267-293](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L267-L293). 1/2/3/4 son cantidades solicitadas de frames generados, conceptualmente x2/x3/x4/x5; no prueban que los frames hayan sido presentados. 5 corresponde conceptualmente a x6, sujeto al límite real.

Cadena distinta: `[DLSSG].OverrideInterpolationCount` modifica la opción de la instancia Streamline del juego; 0 puede apagar FG. También se limita por GetState. No es la misma variable que configura la instancia propia. [OptiScaler/hooks/Streamline_Hooks.cpp:1304-1338](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/Streamline_Hooks.cpp#L1304-L1338). La API final visible es **slDLSSGSetOptions**, no un override mágico de generación NVIDIA en Ampere.

# x3/x4/x5 Generator Ownership

| Selección efectiva | x3/x4/x5: quién genera |
|---|---|
| DLSSG output + replacement None + runtime NVIDIA que admite count | `nvngx_dlssg` NVIDIA, invocado por `sl.dlss_g`; Streamline controla presentación. Es una conclusión del enrutamiento, condicionada a que ese runtime realmente soporte y ejecute la cantidad. |
| DLSSG/NvngxFG con Arturs | DLL `dlss-enabler-headless.dll`; el código de OptiScaler no contiene su algoritmo. La interfaz/menu lo presenta como FSR3 MFG, no como prueba de NVIDIA MFG. |
| FFX/Nukems/Combo | FFX/FSR, Nukem o combinación de proveedores; no convertir el nombre DLSSG de la entrada en NVIDIA real. |

`Nvngx_Arturs::LoadLibraries` resuelve exports `DLSSG_NVSDK_NGX_*` de la DLL Enabler y su clase anuncia hasta cinco fake frames. [OptiScaler/framegen/nvngx/Nvngx_Arturs.cpp:37-70](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_Arturs.cpp#L37-L70); [OptiScaler/framegen/nvngx/Nvngx_Arturs.h:14-18](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_Arturs.h#L14-L18). La descripción de FSR3 MFG/Combo del menú es corroboración, no la única evidencia: el proveedor FFX crea y despacha contextos FidelityFX y devuelve MultiFrameCountMax=1; Combo inicializa Arturs y FFX. [OptiScaler/menu/menu_common.cpp:3355-3361](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/menu/menu_common.cpp#L3355-L3361); [OptiScaler/framegen/nvngx/Nvngx_FFX.cpp:418-468](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_FFX.cpp#L418-L468); [OptiScaler/framegen/nvngx/Nvngx_FFX.cpp:608](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_FFX.cpp#L608); [OptiScaler/framegen/nvngx/Nvngx_FFX.cpp:703](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_FFX.cpp#L703); [OptiScaler/framegen/nvngx/Nvngx_Combo.cpp:21-35](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_Combo.cpp#L21-L35).

NVIDIA real para x3: **CONDITIONAL**. Generador exacto en una ejecución SM86 inexistente: **UNKNOWN**. No se inspeccionaron DLL privadas ni se ejecutaron replacements.

# Streamline Dependency

DLSSG output depende de Streamline: **YES**. Carga explícitamente `streamline/sl.interposer.dll`, sl.common y nvngx_dlssg, inicializa DLSS_G/Reflex/PCL, manual hooking, frame tags y factory proxy. [OptiScaler/proxies/Streamline_Proxy.h:82-99](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/proxies/Streamline_Proxy.h#L82-L99); [OptiScaler/proxies/Streamline_Proxy.h:331-358](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/proxies/Streamline_Proxy.h#L331-L358).

No es un bypass demostrado del gate EVALUATE_NOT_INVOKED. Diferencias reales respecto a nuestro harness: headers 2.11.1, `eUseDXGIFactoryProxy`, `eBlockPresentingClientQueue`, recursos por slots/readiness, opciones/constants preparados en Present, y perspectiva/cámara reconstruida cuando la entrada no aporta matrices. [external/streamline/sl_version.h:24-26](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/external/streamline/sl_version.h#L24-L26); [OptiScaler/proxies/Streamline_Proxy.h:354-358](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/proxies/Streamline_Proxy.h#L354-L358); [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:282-293](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L282-L293); [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:338-450](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L338-L450). Ninguna diferencia prueba por sí sola que Evaluate se active. No se propone otro fix sobre nuestro experimento 2.12.

# DLSS Enabler Dependency

Requerido para el output NVIDIA genuino: **NO como dependencia de código**. Disponible como replacement: **CONDITIONAL**, escogiendo Arturs o Combo. Su papel visible es proveedor externo NGX-compatible, no activador demostrado de NVIDIA MFG en Ampere. [OptiScaler/framegen/nvngx/Nvngx_FG.cpp:24-44](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_FG.cpp#L24-L44); [OptiScaler/framegen/nvngx/Nvngx_Arturs.cpp:37-70](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_Arturs.cpp#L37-L70).

OptiScaler spoofa arquitectura pre-Ada para permitir cargar DLSS_G; esto es un gate de compatibilidad, no prueba de kernels NVIDIA MFG ni de count=2. [OptiScaler/hooks/Streamline_Hooks.cpp:854-883](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/Streamline_Hooks.cpp#L854-L883). Si el soporte de DLSSG real falla y no había replacement, configura Nukems y exige reiniciar: no garantiza NVIDIA como fallback. [OptiScaler/proxies/Streamline_Proxy.h:406-424](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/proxies/Streamline_Proxy.h#L406-L424).

**AMPERE_NATIVE_NVIDIA_MFG_SUPPORTED_BY_OPTISCALER=UNKNOWN**. El código depende del máximo reportado por runtime y de proveedores externos; no demuestra una habilitación NVIDIA x3 específica de SM86 ni permite concluir Blackwell-only a partir de estas fuentes. El componente que puede cambiar la compatibilidad es un runtime NVIDIA/SM86 capaz de ejecutar MFG; Enabler cambia el proveedor, por lo que no satisface por sí solo el objetivo NVIDIA real.

# Native Swapchain / Present Ownership

Present de entrada ocurre en el hilo llamante del juego; `FGHooks::HookFGSwapchain` detoura Present/Present1 y resize/fullscreen. `FGPresent` prepara el output, llama `fg->Present()` y finalmente el original guardado `o_FGSCPresent/Present1`. Para DLSSG ese objeto proviene de la factory Streamline. [OptiScaler/hooks/FG_Hooks.cpp:394-458](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/FG_Hooks.cpp#L394-L458); [OptiScaler/hooks/FG_Hooks.cpp:1245-1275](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/FG_Hooks.cpp#L1245-L1275); [OptiScaler/hooks/FG_Hooks.cpp:1318-1346](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/FG_Hooks.cpp#L1318-L1346).

Ownership: OptiScaler conserva `_swapChain`, HWND y game queue, mientras Streamline es dueño del mecanismo interno de interpolación/presentación generado; DXGI/driver realizan el Present final. [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:166-168](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L166-L168). **No se encontró en este output un hilo OptiScaler que recorra G1/G2/G3 y llame Present por cada imagen.** El hilo exacto, colas y scheduler interno NVIDIA son UNKNOWN desde este repo. No inferirlos del nombre de una opción.

La afirmación general “native swapchains for frame pacing” no queda demostrada aquí como un scheduler NVIDIA propio. El wrapper `LocalPresent` reenvía Present/Present1, y la tunabilidad explícita de pacing hallada pertenece a FidelityFX. [OptiScaler/wrapped/wrapped_swapchain.cpp:272-280](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/wrapped/wrapped_swapchain.cpp#L272-L280); [OptiScaler/framegen/ffx/FSRFG_Dx12.cpp:260-294](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/ffx/FSRFG_Dx12.cpp#L260-L294).

# Frame Pacing

Para NVIDIA DLSSG: plugin/swapchain Streamline + runtime NVIDIA; OptiScaler configura bloqueo de la queue cliente y Reflex low latency, y envía PresentStart/PresentEnd/ReflexSleep en su hook. No contiene una fórmula pública propia para distribuir temporalmente G1..Gn en esta clase. [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:282-304](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L282-L304); [OptiScaler/hooks/FG_Hooks.cpp:1252-1264](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/FG_Hooks.cpp#L1252-L1264); [OptiScaler/hooks/FG_Hooks.cpp:1338-1346](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/FG_Hooks.cpp#L1338-L1346).

Para FSR: `FfxSwapchainFramePacingTuning` y `D3D12_Configure`, con parámetros de spin/sleep. Es una ruta diferente y no evidencia de pacing NVIDIA. [OptiScaler/framegen/ffx/FSRFG_Dx12.cpp:260-294](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/ffx/FSRFG_Dx12.cpp#L260-L294). La presentación generada exacta NVIDIA queda delegada; no reimplementar sólo sleeps suponiendo que así se autoriza G2.

# Required Inputs

| Input | Requerido/opcional en implementación inspeccionada | Fuente OptiScaler | Nuestro proyecto ya lo tiene |
|---|---|---|---|
| Final color/backbuffer | Necesario para imagen/presentación; obtenido del swapchain | DLSSG_Dx12::CreateSwapchain1Internal | YES |
| Depth | Requerido por gate de Dispatch | DLSSG_Dx12.cpp:256-262 | YES |
| MotionVectors + escala | Requerido por gate de Dispatch y constants | DLSSG_Dx12.cpp:256-262,414-425 | YES |
| HUDLessColor | Condicional en host; preservar los requiredTags declarados por cada runtime | DLSSG_Dx12.cpp:311-325,1000-1002 | YES, harness; producción Minecraft requiere routing |
| UIColorAndAlpha | Condicional en host; no sustituir por UIAlpha | DLSSG_Dx12.cpp:1004-1006 | YES, harness; producción requiere routing |
| UIAlpha | No hay mapping independiente en SetResource inspeccionado | DLSSG_Dx12.cpp:994-1013 | NO, tampoco nuevo prerequisito demostrado |
| Jitter/camera/proyección/reset/depth flags | Contrato de constantes usado en dispatch | DLSSG_Dx12.cpp:338-452 | YES en experimentación; validar semántica de cámara real |
| FrameToken/ID | Necesario para constants/tagging | DLSSG_Dx12.cpp:454-468,1031-1040 | YES |
| Reflex/PCL | Configuración/markers/sleep según input del juego | FG_Hooks.cpp:1252-1264,1338-1346 | YES experimental |
| Estados/extent/validez/fences/queue | Necesarios para vida útil GPU; recursos a tiempo | IFGFeature_Dx12.h:15-32; DLSSG_Dx12.cpp:1016-1040,789-815 | YES, interop disponible; adapter nuevo pendiente |

Fuentes: [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:256-262](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L256-L262); [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:311-452](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L311-L452); [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:988-1040](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L988-L1040); [OptiScaler/framegen/IFGFeature_Dx12.h:15-32](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/IFGFeature_Dx12.h#L15-L32). Los gates del host no sustituyen slGetFeatureRequirements de la versión binaria elegida. Infraestructura propia tomada de los baselines ya demostrados, sin nueva ejecución.

# Comparison With Our Current Backend

Baseline proporcionado: PresentWorker es único owner de Present y x2 directo funciona. OptiScaler DLSSG entrega el ownership de generated presentation a Streamline; su `Present()` prepara dispatch y después el hook llama el proxy. [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:826-829](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L826-L829); [OptiScaler/hooks/FG_Hooks.cpp:1275](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/FG_Hooks.cpp#L1275); [OptiScaler/hooks/FG_Hooks.cpp:1322-1326](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/FG_Hooks.cpp#L1322-L1326).

Recomendación **C: backend experimental separado**. Mantener el PresentWorker x2 estable. Para una ruta NVIDIA Streamline, un único owner del Present de entrada y Streamline como scheduler de salidas; nunca presentar las mismas salidas también desde PresentWorker. Para un futuro direct NGX MFG, un scheduler nuevo sólo después de disponer de contratos públicos de cada output/validez; convertir el worker ahora no resuelve ese contrato. Esto es propuesta propia, no una capacidad ya demostrada.

# Comparison With Direct NGX x3

Nuestro baseline histórico produjo Evaluate SUCCESS, MultiFrameCount=2 y una G2 cambiada, pero estado público UNKNOWN/0xffffffff. Se acepta como evidencia existente; no se reanalizó ni ejecutó.

**OPTISCALER_SOLVES_OUR_G2_STATUS_PROBLEM=NO** como contrato público individual. DLSSG_Dx12 no devuelve G1/G2 ni estados NGX por imagen; usa el swapchain Streamline. `hkslDLSSGGetState` observa estado agregado y `numFramesActuallyPresented`, no certifica nuestra G2 directa. [OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp:155-163](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/dlssg/DLSSG_Dx12.cpp#L155-L163); [OptiScaler/hooks/Streamline_Hooks.cpp:1341-1437](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/hooks/Streamline_Hooks.cpp#L1341-L1437). El replacement reenvía parámetros a un proveedor externo; ni eso ni un MultiFrameIndex prueban autorización NVIDIA G2. [OptiScaler/framegen/nvngx/Nvngx_FG.cpp:329-351](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_FG.cpp#L329-L351); [OptiScaler/framegen/nvngx/Nvngx_FG.cpp:398-399](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/OptiScaler/framegen/nvngx/Nvngx_FG.cpp#L398-L399).

Streamline podría absorber esa responsabilidad en otra ruta y permitir medir presentación agregada, pero no aporta el status faltante al backend directo. No declarar G2 autorizada por cambio de sentinel.

# Licensing Boundary

El repo contiene [LICENSE GPL-3.0](https://github.com/optiscaler/OptiScaler/blob/97e99b4c5d9e8af38f14e3b00e1b3f7b35ab5aed/LICENSE). No se copiaron implementaciones ni headers al proyecto principal. Sólo se escriben esta explicación original y result.json con referencias y nombres de APIs. GPL_CODE_REUSE_REQUIRED=NO para el diseño conceptual; CAN_REIMPLEMENT_FROM_PUBLIC_APIS_WITHOUT_COPYING_GPL_CODE=YES para adaptador, slots, instrumentación y ownership. No equivale a poder reimplementar internals privados NVIDIA ni asegurar compatibilidad SM86. La lógica de hooks internos/JSON/SystemCaps observada en OptiScaler no se propone como requisito de nuestro prototipo público.

# Minimum Architecture To Reimplement

Propuesta propia, sin código funcional:

1. FG input adapter para recursos D3D12 ya existentes, cámara/jitter/MV y frame ID.
2. Output NVIDIA intercambiable que conserva procedencia de DLL; replacement None obligatorio.
3. Control de cantidad generada limitado por capacidad real, inicialmente count=1.
4. Owner único de Present de entrada; con Streamline, generated scheduler delegado al plugin.
5. Frame pacing/Reflex públicos, sin sleeps arbitrarios como sustituto del scheduler.
6. Slots GPU, fences, states, extents y lifetimes sin transporte CPU.
7. Telemetría Create/Evaluate/completion/numFramesActuallyPresented, identidad NVIDIA y cierre.
8. Gate de compatibilidad SM86 y versión coherente antes de admitir MFG.

No recrear SR/interoperabilidad existentes; no migrar Minecraft ni sustituir el backend estable.

# Recommended First Prototype

Sólo plan para `experiments/optiscaler-style-dlssg-mfg/`, fuera de Minecraft. Una escena GPU con movimiento coherente, depth/MV/HUDLess/UI y una cámara válida; adapter/output separados, sin hooks de juego o replacements. Conjunto x64 NVIDIA Streamline/NGX coherente distinto del experimento 2.12 cerrado, escogido y fijado antes de build (headers OptiScaler 2.11.1 son referencia de API, no prueba de que sus DLL actuales sean 2.11.1). Reusar nuestro componente SM86 únicamente si se verifica su compatibilidad con ese conjunto y no se presenta como capacidad nativa oficial.

Probar count=1/x2 en una iteración posterior autorizada. Gates: procedencia NVIDIA → soporte real → options → tags y GPU lifetimes → Create → Evaluate>0 → GPU completion → numFramesActuallyPresented>=2 → imagen visible → cierre limpio. STOP al primer fallo, sin fallback a FSR. Sólo tras x2 PASS plantear count=2/x3 en otra iteración. Si no existe un conjunto compatible demostrable, NO construir ni ejecutar a ciegas. No reabrir Streamline 2.12 ni introducir ahora los cambios de opciones observados.

# Go / No-Go Decision

**GO_NO_GO=CONDITIONAL para un prototipo separado, no GO a NVIDIA x3 en SM86.** Condiciones pendientes: seleccionar y fijar un conjunto NVIDIA/Streamline x64 coherente fuera de la ruta 2.12, demostrar compatibilidad del componente SM86 y disponer de contrato público de capacidad y presentación para count=2. OptiScaler aporta una separación útil de inputs/outputs y ownership, pero su output NVIDIA sigue dependiendo de Streamline y no demuestra que evite EVALUATE_NOT_INVOKED en nuestra GPU.

**La respuesta a “¿podemos reimplementar NVIDIA count=2/x3 en SM86 con lo encontrado?” es: no está demostrado.** No se encontró un backend NVIDIA independiente ni un status G2 público nuevo. DLSS Enabler sí ofrece una ruta de sustitución, pero no es evidencia de NVIDIA MFG real y no cumple el objetivo. Para ejecutar el mismo 2.12 o aceptar FSR como NVIDIA, NO_GO.

RUNTIME_EXECUTED=NO; builds=0. No se modificaron Streamline 2.12, PresentWorker, backend directo, AMD, mods ni runtime-baseline. No se ejecutó Minecraft. Sólo dos artefactos textuales nuevos.
