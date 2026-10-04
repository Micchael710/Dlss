# Streamline + SM86 capability harness — 2026-10-04

Start HEAD `38786a2a553ed089fc811c7698b9c4b2e946dfa9`, clean main. **BLOCKED_MISSING_AUTHORIZED_RUNTIME_BINARIES**. No new capability value was obtained; do not infer a maximum from configuration, header defaults, or the historical direct x2 result.

Created `experiments/streamline-mfg-capability`: small D3D12 executable using the existing NVIDIA public Streamline headers, dynamic public core exports and `slGetFeatureFunction` for DLSS-G GetState/SetOptions. It initializes before D3D12 device creation, selects NVIDIA RTX3050Ti explicitly by vendor/name, passes its actual adapter LUID, and tests counts1–4 only up to the queried maximum. No Present, swapchain, rendering, NGX-direct, Vulkan, OpenGL, downloads, OTA, fake capability or proxy configuration is introduced. Loading the proxy is reported separately from independently proving its interception active. SetOptions acceptance would not establish generation or physical native GPU capability.

Build PASS, MSVC19.51.36244.0, local public SDK headers2.12.0. One executable invocation only, ended before DLL loading/GPU/API calls: `MISSING_COMPONENT=sl.interposer.dll`, process exit2, no crash or retry. No swapchain workaround was attempted because the runtime itself is absent.

Inventory of both authorized workspaces `D:/ProjectDllsss` and `D:/ProjectDlssmiAmigo` found no `sl.*.dll` or Streamline-named runtime archive. Available: headers, SM86 proxy0.3.5 (`version.dll` SHA256 `c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838`) and historical NVIDIA DLSS-G components. A DLSS-G NGX runtime is not a substitute for the missing Streamline interposer/plugins. Missing runtime set includes `sl.interposer.dll`, `sl.common.dll`, `sl.dlss_g.dll`; the harness requests Reflex as well, so a future authorized Streamline package must include its required plugin/dependencies. No new package was downloaded under the existing-components-only instruction.

The only new runtime evidence is `logs/research/streamline-mfg-capability/20261004-preflight/{harness.log,result.json,launch-requested.json}`. Executable/build products remain local under ignored builds. GPU name/LUID, actual Streamline runtime version, support, GetState and options are NOT_QUERIED, not historical values relabeled as this run. No assertion that proxy loading alone proves active spoofing. A future maximum, if measured with active SM86 interception, must be labeled `REPORTED_VALUE_VIA_SM86_PROXY`.

No Minecraft, runtime-baseline, .minecraft, PresentWorker, SR, direct x2, direct x3, AMD or NVIDIA binary modifications. Existing x2 PASS preserved. Build and textual preflight are the complete result of this iteration; first blocker STOP.

```text
GPU_NAME=NOT_QUERIED
ADAPTER_LUID=NOT_QUERIED
STREAMLINE_VERSION=NOT_LOADED;HEADERS_2.12.0
STREAMLINE_INIT_RESULT=NOT_CALLED
DLSSG_FEATURE_SUPPORTED=NOT_QUERIED
DLSSG_GET_STATE_RESULT=NOT_CALLED
SM86_COMPONENT_VERSION=0.3.5
SM86_PROXY_LOADED=NO
SM86_PROXY_ACTIVE=NO
SM86_CONFIG_MAX_GENERATED_FRAMES=NOT_LOADED
NUM_FRAMES_TO_GENERATE_MAX=NOT_QUERIED
MAX_VALUE_SOURCE=NONE
SET_OPTIONS_1_RESULT=NOT_CALLED
SET_OPTIONS_2_RESULT=NOT_CALLED
SET_OPTIONS_3_RESULT=NOT_CALLED
SET_OPTIONS_4_RESULT=NOT_CALLED
X2_CAPABILITY=NOT_QUERIED_THIS_HARNESS
X3_CAPABILITY=UNKNOWN
X4_CAPABILITY=UNKNOWN
X5_CAPABILITY=UNKNOWN
X3_RUNTIME_TESTED=NO
X4_RUNTIME_TESTED=NO
X5_RUNTIME_TESTED=NO
MINECRAFT_EXECUTED=NO
X2_BASELINE_MODIFIED=NO
DATA_HARDCODED_OR_FABRICATED=NO
PROCESS_EXIT_CODE=2
IF_FAIL_STAGE=MISSING_AUTHORIZED_RUNTIME_BINARIES
ROOT_CAUSE=STREAMLINE_INTERPOSER_AND_PLUGINS_ABSENT_FROM_WORKSPACES
CAUSE_CONFIDENCE=HIGH_FOR_LOCAL_COMPONENT_INVENTORY
```
