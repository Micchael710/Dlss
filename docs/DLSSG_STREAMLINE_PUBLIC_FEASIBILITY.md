# Streamline DLSS-G public feasibility — 2026-10-04

Start HEAD: `078fec4279251700337c79dda2fcb74bcc6f8b39`, main clean. Stable direct SR+x2 implementation remains untouched. Decision: **BLOCKED_PRESENTATION_CONTRACT**. No capability harness, builds, SetOptions calls or Minecraft run: the presentation gate fails first. Direct NGX status investigation was not reopened.

## Public contract examined

Only NVIDIA public API/guides and the documentation of the existing SM86 component were consulted. Streamline DLSS-G guide identifies itself as **2.14.1**; this is the reviewed documentation version, not an installed/loaded runtime version. Sources use the public main branch as retrieved on 2026-10-04.

| Question | Result and primary source |
|---|---|
| Public MFG parameter | `DLSSGOptions::numFramesToGenerate`: generated frames between fully rendered frames. 1→x2, 2→x3, 3→x4; 4→x5 follows the same documented definition, subject to queried support. [sl_dlss_g.h, lines74–78](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/include/sl_dlss_g.h) |
| Maximum | Call `slDLSSGGetState(viewport,state,options)` and read `DLSSGState::numFramesToGenerateMax`. Query is distinct from `slIsFeatureSupported`; neither was executed here. [Header, lines158–163 and186–220](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/include/sl_dlss_g.h) |
| Graphics API | DLSS-G guide describes D3D12 **and Vulkan** device/feature requirements; D3D11 is not established as a DLSS-G route by these requirements. General Streamline D3D11 support is not proof of DLSS-G support. [DLSS-G guide, sections2.1–3](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/docs/ProgrammingGuideDLSS_G.md) |
| Swapchain / presentation | D3D12 uses a DXGI proxy swapchain and intercepted Present. Vulkan instead intercepts `vkQueuePresentKHR` and `vkAcquireNextImageKHR`; DXGI is not a universal requirement for all Streamline routes. [DLSS-G guide, introduction, sections6.7 and16](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/docs/ProgrammingGuideDLSS_G.md) |
| Options / execution | SetOptions takes effect at a subsequent Present; MFG generation and pacing belong to that presentation path. [DLSS-G guide, sections6.0–6.2](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/docs/ProgrammingGuideDLSS_G.md) |
| Manual integration | Manual hooking uses upgraded interfaces/proxy hooks; it still forwards presentation to Streamline. `slUpgradeInterface` is a DirectX integration mechanism, not an output export API and not mandatory for the Vulkan path. [Manual hooking guide, sections4–5](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/docs/ProgrammingGuideManualHooking.md), [general guide, sections2 and5.1](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/docs/ProgrammingGuide.md) |
| External generated textures | No documented public G1/G2/G3 export, caller-owned generated-output texture or output-lease callback was found in the reviewed DLSS-G API. The public header exposes GetState/SetOptions; completion fence fields protect consumed **inputs**, not generated output ownership. [Public header](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/include/sl_dlss_g.h) |

The public manual-hooking examples are relevant published integration code: they forward DXGI/proxy interface calls and Vulkan Present to Streamline. Two attempted guessed implementation-file URLs were unavailable and provide no evidence; no private implementation or ABI inspection was performed. Conclusion is limited to the reviewed public API/contract, not an assertion about every internal implementation.

## Existing SM86 component

The existing staging tool uses `sdli-0.3.5/version.dll`, SHA256 `c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838`. No component was loaded, replaced or modified this iteration.

The component's [published English README](https://raw.githubusercontent.com/sdli1995/dlssg_for_sm86/main/README.en.md) documents Windows/D3D12 support, architecture rewriting exposed to Streamline/game code, and lack of a maintained Vulkan route. Its configuration ceiling is not a measured Streamline runtime maximum. Any future runtime maximum under this proxy must be labeled `REPORTED_VALUE_VIA_SM86_PROXY`, not native physical GPU capability. No fake architecture/capability was introduced by this project.

## Architectural decision

Current route is OpenGL → Vulkan interop → D3D12 generation → Vulkan output leases → PresentWorker. It relies on direct exported output resources and keeps PresentWorker as the only presenter. Streamline's documented DLSS-G model has no equivalent output-export contract, so it cannot be substituted as this sidecar while preserving that presentation ownership.

A future isolated Streamline backend would require presenting the final SR/HUD result through a Streamline-managed D3D12/DXGI swapchain (or a supported Vulkan presentation route), letting its proxy own generated-frame pacing/presentation. PresentWorker could submit a real frame to that path, but could no longer independently present exported G1/G2 as sole presenter. GPU-only interop, SR coexistence, input lifetime/Reflex, resize and shutdown would then need a separate architectural validation. None was implemented or claimed viable here. The existing SM86 component's documented D3D12 scope additionally prevents assuming its Vulkan path works.

## Result

```text
STREAMLINE_PUBLIC_DLSSG_API=FOUND
STREAMLINE_VERSION=2.14.1_DOCUMENTATION_ONLY
NUM_FRAMES_TO_GENERATE_PARAMETER=PUBLIC;1:x2,2:x3,3:x4,4:x5
NUM_FRAMES_TO_GENERATE_MAX=NOT_QUERIED
MAX_VALUE_SOURCE=NONE_RUNTIME_NOT_INITIALIZED
SM86_PROXY_ACTIVE=NOT_LOADED_THIS_ITERATION
STREAMLINE_REQUIRES_DXGI_SWAPCHAIN=YES_FOR_D3D12;NO_FOR_VULKAN
STREAMLINE_REQUIRES_PRESENT_INTERCEPTION=YES
STREAMLINE_REQUIRES_PRESENT_OWNERSHIP=YES_FOR_GENERATED_FRAME_PACING
STREAMLINE_GENERATED_TEXTURE_EXPORT_AVAILABLE=NO_IN_REVIEWED_PUBLIC_API
CURRENT_PRESENTWORKER_ARCHITECTURE_COMPATIBLE=NO
STREAMLINE_INIT=NOT_ATTEMPTED
DLSSG_SUPPORTED=NOT_QUERIED
SET_OPTIONS_2_RESULT=NOT_ATTEMPTED
SET_OPTIONS_3_RESULT=NOT_ATTEMPTED
SET_OPTIONS_4_RESULT=NOT_ATTEMPTED
X3_RUNTIME_EXECUTED=NO
G1_GENERATED=NOT_TESTED
G2_GENERATED=NOT_TESTED
G1_PRESENTED=NOT_TESTED
G2_PRESENTED=NOT_TESTED
REAL_G1_G2_REAL_OBSERVED=NOT_TESTED
CPU_TRANSPORT_COPY_COUNT=NOT_MEASURED
DEVICE_LOST=NOT_MEASURED
GL_SYNC_ERROR_COUNT=NOT_MEASURED
NORMAL_SHUTDOWN=NOT_APPLICABLE_NO_RUNTIME
DLSS_SR_PLUS_STREAMLINE_DLSSG_X3=BLOCKED_PRESENTATION_CONTRACT
X4_CAPABILITY=NOT_QUERIED
X5_CAPABILITY=NOT_QUERIED
X4_TESTED=NO
X5_TESTED=NO
X6_TESTED=NO
IF_FAIL_STAGE=PUBLIC_PRESENTATION_AND_TEXTURE_EXPORT_CONTRACT
ROOT_CAUSE=NO_PUBLIC_GENERATED_OUTPUT_EXPORT_FOR_CURRENT_PRESENTWORKER
CAUSE_CONFIDENCE=HIGH_FOR_REVIEWED_PUBLIC_CONTRACT
X2_BASELINE_MODIFIED=NO
X2_BASELINE_PRESERVED=YES
DATA_HARDCODED_OR_FABRICATED=NO
CONFIG_RESTORATION=NOT_NEEDED_UNCHANGED
```

Stop at the first incompatible architectural gate; do not infer runtime support or launch a capability harness merely to obtain a maximum that cannot fix this contract.
