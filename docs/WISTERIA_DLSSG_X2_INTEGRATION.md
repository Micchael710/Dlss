# Wisteria DLSS-G x2 — isolated runtime integration

## Embedded debug correction — next authorized phase

Phase base `289862295b04422a41dc0a5a47068ab1b00c9bd4`. The shared public D3D12 initializer now accepts an explicit context. Historical standalone callers keep StandaloneHarness by default and may enable the layer before creating their device. The JNI session explicitly selects EmbeddedMinecraft: it does not call D3D12GetDebugInterface, EnableDebugLayer, DisableDebugLayer or DRED configuration; DXGI factory flags are0. It records `UNAVAILABLE_IN_EMBEDDED_RUNTIME` and `debug_layer_enabled=false` scoped to this initializer. Pre-existing third-party process debug state remains unknown. Debug absence is not a functional failure.

Public CreateDevice audit: adapter is nonnull IDXGIAdapter1 selected by VendorId0x10de, RTX3050Ti name, exact LUID `4c29010000000000`, and no DXGI_ADAPTER_FLAG_SOFTWARE. DeviceId is recorded (prior observed9632/0x25a0). Required feature level stays D3D_FEATURE_LEVEL_12_0; IID/output stay IID_PPV_ARGS(&device). No null adapter, WARP, AMD iGPU, changed feature level or architecture gate patch. Borrowed Vulkan UUID/LUID and driver checks remain unchanged. Exact create HRESULT is now written before throwing so device-creation failures have durable evidence even without an ID3D12Device.

New C++/JNI and Java build PASS; existing 19 SR CPU tests and 7 evidence CPU tests PASS; DlssgContractTest geometry/motion/metadata/ABI PASS. Packaging retains nine AMD Java classes and both AMD DLLs byte-for-byte. New candidate hashes: SR `5c4939db10578db4dde93c2968c697f469ed6adf9f322e72a95660b2b4842848`; Wisteria `7b425231bf36b377fe88a51f415f0f2cf0fa9c5f33e1b62567e609097e3e71ec`. Native-only behavioral correction; PresentWorker, provider output protocol, shared pool, NGX runtime, loader flags and input mappings are unchanged. No closed harness or new GPU test ran during these checks.

Previous FAIL run below remains preserved without modification. New shadow: PASS (`20261004-030130-357`); presentation requires this gate and matching artifact hashes. One attempt in each authorized mode, stop on failure.

### New shadow outcome

Embedded initialization records debug unavailable/not enabled by our code and factory flags0. Same GPU/driver, D3D12CreateDevice S_OK, NGX Init/CreateFeature PASS. Public backend logs show64 named kernels status0; six empty diagnostics excluded under the existing evidence rule. Eight slots/40 resource imports persist for6776 submitted/completed real jobs;27104 input Vulkan blits, no CPU frame transport or per-frame CPU API handoff wait.6775 non-reset completion flags allow generation, independently supported by Vulkan-readback content samples. Three sampled B/G pairs31–33 only; sample31 anchors A, samples32/33 have G!=A/B/sentinel and readable complete world/HUD, no severe corruption. Static-world temporal sanity passes; strong camera/entity/particle quality remains a presentation-phase observation, not established by these hashes.

All shadow PresentWorker callbacks are REAL. DeviceRemovedReason S_OK, no Vulkan device-lost/crash, safe pool drain and orderly world save/shutdown. Original isolated config restored byte-for-byte; AMD hashes unchanged. Full gates/result and GPU-duration summaries in the new run directory. Debug message validation is unavailable in embedded mode; recorder0 is not certified zero debug errors. Prior failure is still consistent with late debug enable: successful before/after result strengthens that hypothesis but does not prove the original device owner/reset mechanism.

Base: `29deca27b6d4a4401699c83be4258a0fa02f10ab`. Prior offscreen gates remain closed; this phase does not rerun them. Read `CURRENT_REAL_PIPELINE.md` for the pre-edit pipeline audit.

## Candidate implementation

`wisteria:dlssg` belongs to its own `wisteria:dlssg_fg` group, uses APPLICATION_MANAGED_ASYNC, and is registered only with `wisteria.dlssg.enabled=true`. Default builds/runtime selection remain unchanged. Shadow is the default experimental mode; only an explicitly selected presentation mode returns generated leases. A failed experiment latches unavailable and does not retry NGX or select AMD.

Own JNI is packaged and extracted automatically into the experiment directory. The identified sdli0.3.5 module and NVIDIA310.9.1 runtime are local launcher inputs with SHA256 checks; community DLLs/models are not packaged in the JAR or Git. Redistribution remains unresolved. The loader writes its public INI, loads the module and retains it until process exit; no private symbols, ABI guesses, architecture patch or OptiScaler are used.

The native session borrows the existing Vulkan instance/device/FG queue, checks the exact UUID/LUID and driver596.49, and creates one persistent D3D12 device/queue/NGX initialization. Extent/format/orientation keys own persistent features and pools; eight slots follow the existing presentation image budget. Five D3D12-owned shared images per slot (color/HUDless/G1 RGBA8, render-depthR32, render-motionRG32) use dedicated D3D12_RESOURCE imports. Resource and fence handles close after permanent import. No handles/devices/images are created per ordinary frame.

## Input mapping and limitations

The preserved test profile uses OpenGL FSR1, so capture owns the four images and has already flipped rows/Y motion. Color/HUDless are display-sized; depth/motion render-sized. Four Vulkan nearest blits copy/convert inputs into the shared pool. Exact blit features, source transfer usage, formats and extents are checked. Source layouts are restored. No CPU transport, output transport copy, custom AMD shader, new swapchain or D3D12 presentation is introduced.

The public NGX matrices receive the measured camera, with OpenGL clip Z converted from [-1,1] to [0,1]; matrix/inverse and both temporal transforms are converted consistently. JOML column-major bytes encode the corresponding row-vector D3D matrix. D3D clip Y remains positive up, matching the validated D3D12 reference. Top-oriented UV motion scales to clip by (2,-2); pixel jitter scales by render size. Depth is normal normalized device depth and camera handedness remains Minecraft's measured convention. Only measured SDR/SRGB is accepted. HDR/luminance and UI alpha are not invented. UI recomposition is explicitly disabled; actual final and HUDless textures are supplied separately.

Jittered producer motion is logged as a quality limitation: no previous jitter or private flag is fabricated. Entity/particle/transparency motion quality remains unproven. Borrowed Vulkan-upscaler inputs are deliberately rejected in this first owned-capture integration: their concurrent main-queue readers require an independently audited queue join before a transfer-layout change. DLSS Super Resolution is untouched.

## GPU handoff and leases

An optional provider-output submission plan consumes capture-ready binary semaphores in native Vulkan input work while holding the existing Java queue submit lock. Input work signals an odd shared-fence value; the D3D12 queue GPU-waits, records Evaluate(count=1,index=1), executes and signals the next even value. The normal worker output command GPU-waits on that value and acquires imported G1/real textures. Its usual binary signals release capture and hand off output to PresentWorker. The following job's odd signal is submitted after this output wait, avoiding timeline regression.

There is no CPU wait between APIs in the ordinary frame path. CreateFeature has a bounded setup-only completion wait. Per-slot allocators reset only after the lease's prior output/presentation fences retire. A single nonblocking completion-value check asserts retirement; it is not a poll loop. Resize creates a new keyed pool and retires the old pool only after all leases drain. Extent mismatch during resize uses real-only discard/reset without executing NGX. Reset, real-frame gaps, discontinuity epoch and provider disable invalidate history. Provider shutdown occurs on the owning FG thread after the existing presenter drain.

Shadow returns a provider-owned real lease and zero generated presentation outputs. NGX still evaluates, writes G1 and completes through Vulkan. Three image samples after initial warmup are recorded by the **Vulkan consumer command** and mapped only after the existing output/presentation fences retire; other frames have no pixel readback. The small disable-interpolation flag and GPU timestamps are diagnostic metadata, not image transport. Both G1 and the current real are hashed; consecutive sample IDs supply the previous-real hash. Capabilities alone never establish success.

Presentation, if shadow passes, uses the same leases and existing PresentWorker: A -> G(A,B) -> B. Reset intervals return real only. PresentWorker's optional observation callback records successful present requests without changing acquisition, pacing or ownership. Queue delay is CPU batch-publication to worker entry; this is not display latency. GPU input timing includes copies, barriers and the G1 sentinel clear. DLSS-G timing uses D3D12 GPU timestamps. Added end-to-end display latency requires a matched baseline/display measurement and stays UNKNOWN otherwise.

## Validation status

- New C++/JNI build: PASS (MSVC19.51 x64, release).
- Java21 candidate build/packaging: PASS. SR candidate SHA256 `11575418ba744d171ce6cb488c76d0e895a94d7786267afe5d8c7c1eae3ea0fd`; Wisteria candidate `7c447825deafeac15dad762e25a502dd3389087b5bbdcfea4246d03e436de22c`. Nine AMD Java classes and both packaged AMD native DLLs are byte-for-byte identical to the preserved Wisteria baseline; both baseline JAR hashes and the original AMD bridge file remain identical.
- CPU camera near/far, inverse, DX Y/motion conventions, jitter normalization, immutable constants, reset matrices and own JNI ABI/linkage: PASS. No NGX/GPU/community initialization in those CPU checks.
- Minecraft shadow: **FAIL**, único run `20261004-010600-001`. Generated presentation: **NOT_RUN_SHADOW_GATE_FAILED**.
- Prior AMD x2, DLSS-G D3D12 x2, shared interop and end-to-end offscreen x2 remain prior PASS evidence, not new Minecraft results.

Sources for the transport contract: [Vulkan blits and format conversion](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html), [D3D12 queue/fence synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization), plus the pinned local NVIDIA public DLSS-G headers and closed-gate source/evidence. No inference here establishes runtime correctness before the one shadow run.

## Single shadow attempt — closed FAIL, no retry

Minecraft opened the preserved world, active Complementary, SR1.7, FSR1/OpenGL and Vulkan presentation. Real capture logged five metadata samples with final/HUDless/depth/motion present; own JNI and the public external loader succeeded. Observed OpenGL/Vulkan UUID `01895b66d1ca454d88788dd21fdef638`, DXGI selected LUID `4c29010000000000`, driver596.49. No private backend call, OptiScaler or closed-gate rerun occurred.

Session initialization failed at **D3D12CreateDevice=0x887A0007**. This is [DXGI_ERROR_DEVICE_RESET](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error). NGX Init, CreateFeature, kernel creation, persistent pool/shared fence, Evaluate, GPU copies and G1 never ran in this attempt. Accepted DLSS-G real jobs0, generated0, Evaluate failures0, initialization failures1. The zero Evaluate failures means **not reached**, not successful Evaluate. General Minecraft real-frame/FPS counts were not measured by this provider.

Provider availability latched unhealthy; negotiation changed to fg=null and there was no automatic AMD fallback/retry. Visible world remained behind the settings UI; no generated-frame visual validation is possible. No new crash report or Vulkan device-lost message was observed. The D3D12 reset HRESULT is retained as a failure, not hidden by Minecraft continuing to render. No ID3D12Device was obtained, so GetDeviceRemovedReason and per-device debug queue messages are unavailable. Vulkan validation was not enabled, so error count is unmeasured. Client closed orderly, saved all world dimensions, destroyed Vulkan and launcher finished normally. Presentation run was not started.

### Cause confidence and next step

The reused standalone initializer in `experiments/dlssg-external-harness/d3d12_worker.inc:27` enables the D3D12 debug layer after the external module is loaded, during the first eligible Minecraft FG job. Its `D3D12CreateDevice` call at line35 then fails. [Microsoft's public contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nf-d3d12sdklayers-id3d12debug-enabledebuglayer) requires enabling debug before device creation and documents device removal when enabled afterwards. Graphics/overlay/D3D12 modules were already present in the process; **module presence does not prove an earlier D3D12 device**. Thus late debug enable is a plausible integration-specific cause, medium confidence; failing stage/HRESULT are confirmed with high confidence. Underlying driver reset or another device owner remains unresolved. No claim that SM86/NGX kernels failed: no such call occurred.

Next implementation should separate standalone debug bootstrap from embedded session initialization and never enable the debug layer late. If debug validation is needed, arrange it before any process D3D12 device or record it unavailable; do not change global driver settings. This correction is **proposed only**, not applied after the failed run. Do not claim shared-pool/sync/resize/provider-switch runtime PASS from compiled source. Those paths are implemented but not reached in this attempt; lifetime behavior remains runtime-unproven.

Evidence: `logs/runtime/minecraft-dlssg-x2/20261004-010600-001/` contains immutable original event/launcher logs, run-manifest, result, real capture sequence, unavailable-timing/sync/pool records and unchanged baseline hashes. Three sampled pixel readbacks were planned; none occurred because setup failed. One local UI screenshot is ignored by Git. Original isolated SR config restored byte-for-byte (SHA256 `834981a0d32679bfd99b567150179a9d279ab0780d81949c6297f94c3493f0ee`). No binary payload or runtime world is published.

## Frame4037 review and completion gating

Same interval/resource confirmed in logs; NGX disable1 was consumed only at lease retirement, after a GENERATED present callback. No pixel readback: no corruption claim or causal attribution to perceived cutting. Interrupted presentation retains INCOMPLETE, not technical FAIL/PASS. New generic readiness/validity hook preserves AMD defaults, filters disabled candidates before acquiring a swapchain target, drains every submitted semaphore/fence, and uses an asynchronous D3D12 completion notification. No per-frame API handoff CPU fence wait. See DLSSG_MFG_RUNTIME_VALIDATION.md.
