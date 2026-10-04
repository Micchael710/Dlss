# Wisteria DLSS-G x2 — isolated runtime integration

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
- Minecraft shadow: NOT_RUN. Minecraft generated presentation: NOT_RUN.
- Prior AMD x2, DLSS-G D3D12 x2, shared interop and end-to-end offscreen x2 remain prior PASS evidence, not new Minecraft results.

Sources for the transport contract: [Vulkan blits and format conversion](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html), [D3D12 queue/fence synchronization](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization), plus the pinned local NVIDIA public DLSS-G headers and closed-gate source/evidence. No inference here establishes runtime correctness before the one shadow run.
