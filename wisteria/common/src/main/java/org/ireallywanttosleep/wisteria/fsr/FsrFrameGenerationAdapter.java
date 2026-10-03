package org.ireallywanttosleep.wisteria.fsr;

import io.homo.superresolution.api.registry.framegeneration.*;
import io.homo.superresolution.core.graphics.vulkan.*;
import org.ireallywanttosleep.wisteria.Wisteria;
import org.lwjgl.system.MemoryStack;
import org.lwjgl.vulkan.*;
import java.util.*;
import static org.lwjgl.vulkan.VK10.*;

/** Session/output state will be created only on FrameGenerationWorker. */
public final class FsrFrameGenerationAdapter implements AutoCloseable {
    private volatile boolean historyInvalid = true;
    private Thread owner;
    private FsrSession session;
    private FsrOutputPool pool;
    private Key key;
    private long epoch = -1, previousFrameId = -1, generatedFrames;
    private String lastFailure;
    private record Key(int width, int height, int rw, int rh, int outputFormat, int hudlessFormat) {}
    public void invalidateHistory() { historyInvalid = true; }
    public FrameGenerationDispatchResult dispatch(FrameGenerationDispatchInput input) {
        requireOwner();
        FsrOutputPool.Slot slot = null;
        Map<VulkanTexture, Integer> oldLayouts = new IdentityHashMap<>();
        try {
            if (!(input.providerInputSnapshot() instanceof FsrInputSnapshot snapshot)
                    || snapshot.metadata() == null) throw new IllegalStateException("Immutable real-frame metadata missing");
            var m = snapshot.metadata();
            var c = snapshot.constants();
            if (!m.color().transferFunction().equals("SRGB"))
                throw new IllegalStateException("Unsupported/unavailable declared color transfer: " + m.color());
            if (!Double.isFinite(m.viewSpaceToMetersFactor()) || m.viewSpaceToMetersFactor() <= 0)
                throw new IllegalStateException("View-space conversion unavailable");
            var frame = input.frameResources();
            var color = frame.finalColorVulkanTexture();
            var hudless = frame.hudlessColorVulkanTexture();
            var depth = frame.depthVulkanTexture();
            var motion = frame.motionVectorVulkanTexture();
            if (!frame.hasFinalColor() || !frame.hasHudlessColor() || !frame.hasDepth() || !frame.hasMotionVector())
                throw new IllegalStateException("Required FG image missing");
            if (color.getWidth() != input.outputWidth() || color.getHeight() != input.outputHeight()
                    || hudless.getWidth() != input.outputWidth() || hudless.getHeight() != input.outputHeight()
                    || depth.getWidth() != m.renderSize().width() || depth.getHeight() != m.renderSize().height()
                    || motion.getWidth() != depth.getWidth() || motion.getHeight() != depth.getHeight())
                throw new IllegalStateException("Snapshot/image/swapchain extent mismatch");
            if (c.depthInverted() != 0 || c.motionVectorsJittered() != 0)
                throw new IllegalStateException("This first SDR path expects finite normal depth and unjittered motion");
            var desired = new Key(input.outputWidth(), input.outputHeight(), depth.getWidth(), depth.getHeight(),
                    color.getTextureFormat().vk(), hudless.getTextureFormat().vk());
            if (!desired.equals(key)) {
                if (session != null) { input.device().requireFgQueue().waitIdle(); session.close(); }
                session = new FsrSession(FidelityFxBridge.createSession(input.device().getVkDevice().address(),
                        input.device().getPhysicalDevice().address(), VK.getFunctionProvider().getFunctionAddress("vkGetDeviceProcAddr"),
                        desired.width, desired.height, desired.rw, desired.rh, desired.outputFormat, desired.hudlessFormat));
                key = desired; historyInvalid = true;
                Wisteria.LOGGER.info("FSR_SESSION created key={}", key);
            }
            if (pool == null) pool = new FsrOutputPool();
            slot = pool.acquire(input.device(), input.outputWidth(), input.outputHeight(), desired.outputFormat, 1);
            var output = slot.outputs(1).getFirst();
            var real = slot.realOutput();
            for (var texture : List.of(color, hudless, depth, motion, output, real)) oldLayouts.put(texture, texture.getCurrentLayout());
            VkCommandBuffer cb = new VkCommandBuffer(input.commandBuffer(), input.device().getVkDevice());
            for (var texture : List.of(color, hudless, depth, motion)) transition(cb, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            transition(cb, output, VK_IMAGE_LAYOUT_GENERAL);
            boolean reset = historyInvalid || snapshot.historyResetRequested() || epoch != m.discontinuityEpoch()
                    || previousFrameId + 1 != m.monotonicFrameId();
            float[] params = {c.jitterOffsetX(), c.jitterOffsetY(), depth.getWidth(), depth.getHeight(),
                    (float)m.realFrameDeltaMs(), c.cameraNear(), c.cameraFar(), c.cameraFov(), (float)m.viewSpaceToMetersFactor(),
                    c.cameraPosX(), c.cameraPosY(), c.cameraPosZ(), c.cameraUpX(), c.cameraUpY(), c.cameraUpZ(),
                    c.cameraRightX(), c.cameraRightY(), c.cameraRightZ(), c.cameraFwdX(), c.cameraFwdY(), c.cameraFwdZ()};
            FidelityFxBridge.prepare(session.handle(), input.commandBuffer(), m.monotonicFrameId(), params,
                    resource(depth), resource(motion), resource(hudless));
            FidelityFxBridge.generate(session.handle(), input.commandBuffer(), m.monotonicFrameId(),
                    resource(color), resource(output), reset);
            // Keep a provider-owned real image: imported capture slots can retire on the FG fence,
            // independently of the paced presentation of this complete real/generated pair.
            transition(cb, color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            transition(cb, real, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            try (var stack = MemoryStack.stackPush()) {
                var copy = VkImageCopy.calloc(1, stack);
                copy.get(0).srcSubresource(r -> r.aspectMask(VK_IMAGE_ASPECT_COLOR_BIT).mipLevel(0).baseArrayLayer(0).layerCount(1))
                        .dstSubresource(r -> r.aspectMask(VK_IMAGE_ASPECT_COLOR_BIT).mipLevel(0).baseArrayLayer(0).layerCount(1))
                        .extent(e -> e.width(color.getWidth()).height(color.getHeight()).depth(1));
                vkCmdCopyImage(cb, color.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        real.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy);
            }
            transition(cb, real, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            // Restore imported inputs to the capture/presenter's documented layout before its real blit.
            for (var texture : List.of(color, hudless, depth, motion)) transition(cb, texture, oldLayouts.get(texture));
            epoch = m.discontinuityEpoch(); previousFrameId = m.monotonicFrameId(); historyInvalid = false;
            long generatedId = ++generatedFrames;
            Wisteria.LOGGER.info("FSR_INTERVAL realFrameId={} generatedFrameId={} timestamp={} deltaMs={} reset={} prepareResult=OK dispatchResult=OK outputSlot={} VkImage={} interpolation=0.5 generatedRecorded={}",
                    m.monotonicFrameId(), generatedId, m.realFrameTimestamp(), m.realFrameDeltaMs(), reset,
                    pool.indexOf(slot), output.handle(), generatedFrames);
            FsrOutputPool.Slot acquired = slot;
            return FrameGenerationDispatchResult.success(1, new FrameGenerationProviderOutput() {
                private boolean released;
                public List<VulkanTexture> generatedOutputs() { return acquired.outputs(1); }
                public VulkanTexture realOutput() { return acquired.realOutput(); }
                public FrameGenerationDispatchCompletion completion() { return FrameGenerationDispatchCompletion.completed(); }
                public OutputKey outputKey() { return new OutputKey(input.outputWidth(), input.outputHeight(), desired.outputFormat); }
                public boolean isReleased() { return released; }
                public void release() {
                    requireOwner(); if (released) return;
                    released = true; pool.release(acquired);
                    Wisteria.LOGGER.info("FSR_COMPLETION realFrameId={} generatedFrameId={} slot={} GPU_and_PresentWorker_retired=true",
                            m.monotonicFrameId(), generatedId, pool.indexOf(acquired));
                }
                public void abort() {
                    requireOwner(); if (released) return;
                    oldLayouts.forEach(VulkanTexture::setCurrentLayout);
                    released = true; pool.release(acquired); historyInvalid = true;
                }
            }, reset ? FrameGenerationDispatchResult.HistoryDisposition.RESET : FrameGenerationDispatchResult.HistoryDisposition.UNCHANGED);
        } catch (RuntimeException error) {
            oldLayouts.forEach(VulkanTexture::setCurrentLayout);
            if (slot != null) pool.release(slot);
            historyInvalid = true;
            if (!Objects.equals(lastFailure, error.toString())) {
                lastFailure = error.toString(); Wisteria.LOGGER.error("FSR_DISPATCH unavailable: {}", lastFailure, error);
            }
            return FrameGenerationDispatchResult.failed(error.toString());
        }
    }
    private static long[] resource(VulkanTexture texture) {
        return new long[]{texture.handle(), texture.getTextureFormat().vk(), texture.getWidth(), texture.getHeight()};
    }
    private static void transition(VkCommandBuffer cb, VulkanTexture texture, int layout) {
        int old = texture.getCurrentLayout(); if (old == layout) return;
        try (var stack = MemoryStack.stackPush()) {
            var b = VkImageMemoryBarrier.calloc(1, stack);
            b.get(0).sType(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER).oldLayout(old).newLayout(layout)
                    .srcAccessMask(old == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)
                    .dstAccessMask(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)
                    .srcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED).dstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                    .image(texture.handle()).subresourceRange(r -> r.aspectMask(texture.getAspectMask())
                            .baseMipLevel(0).levelCount(1).baseArrayLayer(0).layerCount(1));
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, null, null, b);
        }
        texture.setCurrentLayout(layout);
    }
    private void requireOwner() {
        if (owner == null) owner = Thread.currentThread();
        if (Thread.currentThread() != owner) throw new IllegalStateException("FSR accessed off owning FG worker");
    }
    public void close() {
        requireOwner(); if (pool != null) pool.close(); if (session != null) session.close();
        pool = null; session = null; key = null;
    }
}
