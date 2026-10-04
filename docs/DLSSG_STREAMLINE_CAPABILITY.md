# Streamline + SM86 capability harness — 2026-10-04

## Current result — real maximum4, options1–4 accepted, no generation test

Start `dd0262a544e530e97f5c0f4d0df1dcd06ba41c3f`, main clean. Direct x2 uses `NVSDK_NGX_D3D12_Init_with_ProjectID`, GUID `3e6891d2-09ac-4f54-ae8d-f481c3150d4b`, CUSTOM engine, version1.0.0; no ApplicationID. Source `experiments/dlssg-external-harness/d3d12_worker.inc:63` is included by the harness and direct bridge. Historical combined x2 evidence confirms NGX Init PASS. Public SDK2.12 source in the supplied ZIP (`source/plugins/sl.common/commonEntry.cpp:1490–1509`) maps nonempty projectId/engineVersion to that ProjectID API. This is not an ApplicationID substitution or newly invented identity.

Only harness change: Preferences.projectId uses that GUID; engineVersion uses1.0.0. Engine CUSTOM unchanged. slInit already precedes device creation; slSetD3DDevice was already present and succeeding after creating the explicit RTX3050Ti device, so that call was not changed. Same SDK2.12.0.17026aaf3, SM86 0.3.5 and runtime binary hashes/layout/MaxGeneratedFrames4. One rebuilt-harness execution with temporary SpoofArchToGame1, no retries or swapchain/Present.

Actual results: Init0, slSetD3DDevice0, feature support0, GetState0, DLSSGStatus0. **GetState.numFramesToGenerateMax=4**, sourced from the real API with SM86 active; label `REPORTED_VALUE_VIA_SM86_PROXY`, not physical native Ampere capability. SetOptions counts1,2,3,4 each return0. Hence reported/options capability x2/x3/x4/x5 YES. It is **not** a frame-generation/runtime presentation PASS. Options take effect at a future Present, which was not called.

Device NVIDIA GeForce RTX3050Ti Laptop GPU, LUID `4c29010000000000`. SL reports architecture0x1b0 and DLSS-G hardware mask0x1. NGX log resolves the existing GUID to a CMS identity, DLSS-G startup succeeds and NGX shutdown is logged: context creation is supported by these events plus successful feature/state queries. Shutdown0/process exit0. INI restored byte-identically to SpoofArchToGame0, SHA256 `5bc366c91027aab6aed8f6b59947b284047e194a0a315eb9a60c71df4d2233e8`.

STOP after capability/options. No Minecraft, generated frames, x3/x4/x5 runtime, PresentWorker/direct x2/SR/AMD changes. Evidence: `logs/research/streamline-mfg-capability/20261004-ngx-identity/` (actual harness/SL logs, structured result, ProjectID provenance, same-DLL hashes, config restoration). The unchanged INI logging path emitted this run's SM86 backend/loader logs under `20261004-sdk212/backend/`; they confirm installed hooks, zero feature creates/evaluates and bundled runtime redirection. Actual loaded bundled-runtime hashes/version are recorded separately. ZIP/DLLs/executable/temporary INI are not published. Previous blockers below remain historical.

## Current result — single SpoofArchToGame1 test, new identity/NGX blocker

Start `3dace051fee6a307fc68d816892f8d8e346d55ee`, clean main. Same executable, runtime DLL hashes, adapter, loading mechanism and MaxGeneratedFrames4. Only temporary isolated INI change: SpoofArchToGame0→1. No build/source changes or additional research. One execution, no retry. Original INI restored exactly: SHA256 `5bc366c91027aab6aed8f6b59947b284047e194a0a315eb9a60c71df4d2233e8`. Temporary test INI is not published; only hashes/results.

Init0; RTX3050Ti LUID `4c29010000000000`; SM86 loaded. Actual SL log comparison: architecture0x170→0x1b0, DLSS-G adapter mask0x0→0x1. This demonstrates proxy effect on Streamline's reported architecture/hardware mask. It does **not** demonstrate a successful public feature-support query: that changed6→32 (`eErrorFeatureNotSupported`). `SM86_PROXY_ACTIVE_FOR_STREAMLINE_FEATURE_GATE=NO_OR_UNPROVEN`, because the required SUCCESS was not observed.

New log blocker: SL requests a correct application ID, reports failed NGX initialization and disables NGX-based features; DLSS-G reports missing NGX context. STOP at FEATURE_SUPPORT. No application-ID workaround, GetState, SetOptions, swapchain, frames or Minecraft. Max/capabilities remain UNKNOWN, not a claim that MFG is impossible. SL shutdown0; process exit1, functional failure. Baseline x2/SR/AMD/PresentWorker untouched. Evidence: `logs/research/streamline-mfg-capability/20261004-spoofarch1/` with actual SL/harness logs, DLL/executable hashes, result and byte-identical restoration.

## Current result — SDK 2.12.0 provided, BLOCKED_FEATURE_SUPPORT

Start HEAD `a136e68a9fc7fcef2443ae9c95e25df706389188`, main clean. Used the user-authorized `streamline-sdk-v2.12.0.zip` directly, no download. Matched24 local headers; staged only production `bin/x64` DLLs after checking AMD64 PE machine0x8664. Normal loader dependency inspection identified `NvLowLatencyVk.dll` in sl.common, included from the same package. Streamline interposer/common/DLSS-G/Reflex/PCL file versions2.12.0; packaged NGX DLSS-G file version310.7.0.0 is recorded separately, not mislabeled310.9.1. All staged DLL hashes and DLLs observed loaded are in the evidence. Proprietary/runtime/build files remain ignored and local.

Reused historical `ExternalLoader`: absolute `LoadLibraryW`, adjacent component INI, bundled mode, Optimized0, RouterSM86, KernelImageAuto, SpoofArchToGame0, diagnostic/cache directories. Isolated MaxGeneratedFrames4 is only a configuration ceiling, never a capability answer. Component0.3.5 hash remains `c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838`. No binary patch, new architecture spoof, private ABI or investigation of NGX-direct status. Runtime directory is isolated from the baseline.

One principal execution after build PASS. `slInit=0 (eOk)`, actual runtime `2.12.0.17026aaf3`. Explicit device NVIDIA GeForce RTX3050Ti Laptop GPU, vendor4318/device9632, LUID `4c29010000000000` matches the historical baseline. D3D12 device creation and slSetD3DDevice both succeed. `slIsFeatureSupported(kFeatureDLSS_G)=6 (eErrorNoSupportedAdapterFound)`. Streamline log reports architecture0x170, DLSS-G hardware mask0x0 and unloads the plugin as unsupported; min-spec requirements query from NGX failed and SL defaults were used. This proves the feature gate failed, not that MFG is impossible in every configuration.

SM86 DLL loading YES; its interception/support effect on Streamline **NOT_DEMONSTRATED**. Why this historical load/config layout did not establish Streamline support is UNKNOWN. No assumptions from loaded modules, configuration ceiling or prior direct x2 result. GetState, status, max and SetOptions1–4 NOT_CALLED/NOT_QUERIED. Capability x2/x3/x4/x5 remains UNKNOWN for this harness. No retry or attempted fix after Init. STOP at FEATURE_SUPPORT.

Process exit1 (reported functional failure, no native crash); slShutdown0. No Present/generated frames/Minecraft, no PresentWorker/SR/x2/AMD/runtime-baseline changes or CPU frame transport. Text evidence: `logs/research/streamline-mfg-capability/20261004-sdk212/{harness.log,sl.log,result.json,staged-sdk.json,dll-versions.json,import-dependencies.txt,process-result.json}`. All binary/hash provenance is preserved; the ZIP and DLLs are not published. Earlier preflight below is historical.

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
