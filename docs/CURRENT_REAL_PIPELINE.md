# Current Minecraft pipeline — DLSS-G integration audit

Audited at `29deca27b6d4a4401699c83be4258a0fa02f10ab`, before changing Wisteria. This is an integration contract, not a new runtime PASS.

## Capture and immutable inputs

`FrameCaptureManager` captures HUDless color before the HUD and final color afterwards. `FrameResources.seal()` attaches the immutable `RealFrameMetadata` and `FrameGenerationConstants` captured by `FGConstantsFeature`. The latter snapshots projection/inverse, previous/current clip transforms, camera vectors, jitter, near/far, field of view, timing, monotonic ID and discontinuity epoch. No invented HDR metadata is available: Iris color-space reflection can yield UNKNOWN.

`GlVulkanInteropAlgorithm.getInteropResourceRequirements()` requests R32F depth and RG16F motion at render size. Final and HUDless color use display size. Actual borrowed images/formats/extents must be checked at dispatch; the configuration alone is insufficient. Owned capture flips image Y and negates motion Y; borrowed algorithm images do so only when the configured interop flip is enabled. The existing isolated profile has that flip disabled, so the DLSS-G input copy must flip borrowed depth/motion rows and compensate motion Y scale. The canonical motion convention is previous-minus-current UV. `FGConstantsBuilder` stores JOML column-major OpenGL projection matrices without a DX clip-space conversion. NGX metadata conversion must account for this explicitly; the synthetic harness constants must not be reused for Minecraft.

The existing GL/Vulkan opaque-Win32 allocations are not D3D12-owned resources. The new provider therefore needs four GPU input copies/conversions: final color, HUDless color, depth and motion. Imported D3D12-owned G1 should be leased directly: zero output transport copies and zero CPU frame transport. A sampled diagnostic readback is separate from transport.

## Owners and ordering

`CaptureFrameRing.MAX_IN_FLIGHT_FRAMES = 3`. The render thread captures a provider snapshot and enqueues `FrameGenerationWork`. `FrameGenerationWorker` alone dispatches providers on `SR-FrameGeneration-FG`; its generation queue capacity is two. Providers receive already-begun Vulkan command buffers and return `FrameGenerationProviderOutput` leases. Existing AMD records its prepare/dispatch and real-color preservation copy into the supplied command buffer.

The current worker submits these buffers with capture ready semaphores, provider-output binary handoffs, and capture release semaphores. A provider-owned real output allows capture retirement after generation completion, independently of paced presentation. `PresentWorker`, on its own thread, consumes generated outputs followed by the current real output. Since the previous real was presented by the previous batch, the sequence is A, G(A,B), B. It alone acquires/presents the existing Vulkan swapchain. Provider output retirement runs on the FG thread after both GPU work and presentation retire.

The presentation queue budget is six images, plus one active presentation batch and one currently-dispatching batch. Queued generation work has no provider leases. A conservative pool bound is `floor(6 / imagesPerBatch) + 2`: eight slots in shadow mode (one real per batch), five for x2 presentation (generated plus real). Allocate eight to serve both modes; do not infer pool capacity from the capture ring alone.

## Required isolated extension

The existing one-submit contract cannot order Vulkan input writes, D3D12 Evaluate and Vulkan G1 consumption. A provider-specific GPU submission plan must perform the input submission first, consuming the capture ready semaphores exactly once. It signals a permanently imported D3D12 shared timeline fence. The D3D12 queue waits on that value, executes NGX and signals completion. The normal worker output submission then waits on completion and emits its existing binary output/capture handoffs.

All Vulkan queue submissions, including native input work, must use `VulkanQueue.submitLock()` because the queue is externally synchronized. No CPU wait is permitted between these API stages. A shared-fence value must not regress: on the single FG queue, each job's output wait is submitted before the next job's input signal. Slot command allocators and images can be reused only after the lease retires. Setup/teardown waits are bounded and outside the per-frame path.

This requires a default-empty submission-plan hook in the provider-output API, an opt-in timeline-wait submission overload, and a non-owning VulkanTexture wrapper for native-owned images. Existing providers retain their old path. No changes to AMD source, ring behavior, PresentWorker ownership or swapchain are intended.

## Lifecycle and test gates

The new experimental provider `wisteria:dlssg` is disabled by default. Controlled local properties select shadow or presentation mode and identified external runtime files; community binaries are not redistributed. D3D12 device/queue/NGX initialization are persistent; features/pools are keyed by render/display extents and formats. Resize, discontinuity, dropped real inputs and provider transitions invalidate history. Old pools remain alive until every lease retires.

Only consecutive original real inputs seed NGX history. Shadow mode evaluates x2 but leases only the preserved real output to PresentWorker. Generated images remain internal and are sampled sparingly after GPU completion. One shadow run is allowed. A failed gate stops the experiment with its precise stage and evidence. Only a successful shadow gate authorizes the one presentation run. Harness PASS results are retained as prior evidence and are not rerun.
