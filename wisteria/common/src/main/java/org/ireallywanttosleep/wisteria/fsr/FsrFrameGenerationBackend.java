package org.ireallywanttosleep.wisteria.fsr;

import io.homo.superresolution.api.registry.framegeneration.*;
import io.homo.superresolution.common.framegeneration.FrameGenerationMode;
import io.homo.superresolution.core.RenderSystems;

public final class FsrFrameGenerationBackend implements FrameGenerationProvider {
    private final FsrFrameGenerationAdapter adapter = new FsrFrameGenerationAdapter();
    public FrameGenerationExecutionModel executionModel() { return FrameGenerationExecutionModel.APPLICATION_MANAGED_ASYNC; }
    public void initialize() {
        FidelityFxBridge.load();
        if (FidelityFxBridge.isAvailable() && Boolean.getBoolean("wisteria.fsr.contextProbe")) {
            var device = RenderSystems.vulkan().device();
            Thread probe = new Thread(() -> {
                try {
                    var session = new FsrSession(FidelityFxBridge.createSession(
                            device.getVkDevice().address(), device.getPhysicalDevice().address(),
                            org.lwjgl.vulkan.VK.getFunctionProvider().getFunctionAddress("vkGetDeviceProcAddr"),
                            854, 480, 502, 282, org.lwjgl.vulkan.VK10.VK_FORMAT_R8G8B8A8_UNORM,
                            org.lwjgl.vulkan.VK10.VK_FORMAT_R8G8B8A8_UNORM));
                    org.ireallywanttosleep.wisteria.Wisteria.LOGGER.info("FSR_CONTEXT_PROBE create=PASS handle={}", session.handle());
                    session.close();
                    org.ireallywanttosleep.wisteria.Wisteria.LOGGER.info("FSR_CONTEXT_PROBE destroy=PASS");
                } catch (Throwable error) {
                    org.ireallywanttosleep.wisteria.Wisteria.LOGGER.error("FSR_CONTEXT_PROBE failed", error);
                }
            }, "FsrContextProbe");
            probe.start();
        }
    }
    public void shutdownOnFrameGenerationThread() { adapter.close(); }
    public void shutdown() {}
    public boolean isAvailable() { return FidelityFxBridge.isAvailable() && RenderSystems.vulkan() != null; }
    // Containers support 0..5; the pinned SDK path currently validates only one output.
    public int supportedGeneratedFrameCount() { return isAvailable() ? 1 : 0; }
    public int presentationManagedGeneratedFrameCount(FrameGenerationMode mode) { return Math.min(mode.generatedFrameCount(), supportedGeneratedFrameCount()); }
    public boolean isDependenciesSatisfied() { return isAvailable(); }
    public void disable() { adapter.invalidateHistory(); }
    public ProviderInputSnapshot captureInputSnapshot(String providerId,
            io.homo.superresolution.common.presentation.capture.FrameResources frame,
            io.homo.superresolution.common.framegeneration.constants.FrameGenerationConstants constants,
            FrameGenerationMode mode) {
        return new FsrInputSnapshot(providerId, frame.logicalFrameIndex(), mode, constants,
                constants.reset() != 0, frame.metadata());
    }
    public FrameGenerationDispatchResult dispatchAsync(FrameGenerationDispatchInput input) { return adapter.dispatch(input); }
}
