/*
 * Super Resolution
 * Copyright (c) 2025-2026. 187J3X1-114514
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

package io.homo.superresolution.common.presentation.capture;

import io.homo.superresolution.common.presentation.vulkan.FramePacingTiming;
import io.homo.superresolution.core.graphics.impl.texture.ITexture;
import io.homo.superresolution.core.graphics.opengl.texture.GlImportableTexture2D;
import io.homo.superresolution.core.graphics.vulkan.VkGlInteropSemaphore;
import io.homo.superresolution.core.graphics.vulkan.VulkanCommandBuffer;
import io.homo.superresolution.core.graphics.vulkan.VulkanDevice;
import io.homo.superresolution.core.graphics.vulkan.VulkanTexture;
import org.lwjgl.system.MemoryStack;
import org.lwjgl.vulkan.VkSemaphoreWaitInfo;


import static org.lwjgl.vulkan.VK10.VK_SUCCESS;
import static org.lwjgl.vulkan.VK12.VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
import static org.lwjgl.vulkan.VK12.vkWaitSemaphores;

public final class FrameResources {
    private final int index;
    private final VulkanDevice device;
    private final FramePacingTiming framePacingTiming;
    private final FrameTextureResource finalColor;
    private final FrameTextureResource hudlessColor;
    private final FrameTextureResource depth;
    private final FrameTextureResource motionVector;
    private final FrameResourceLifecycle lifecycle = new FrameResourceLifecycle();
    private final Object borrowedInputReleaseMonitor = new Object();
    private volatile VulkanCommandBuffer submittedCommandBuffer;
    private volatile long submittedCommandBufferGeneration;
    private volatile long fence;
    private volatile long generation;
    private volatile int logicalFrameIndex;
    private volatile io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata metadata;
    private volatile boolean unrecoverable;
    private volatile long dlssGInputCompletionSemaphore;
    private volatile long dlssGInputCompletionValue;
    private boolean borrowedInputReleaseRequired;
    private boolean borrowedInputReleaseSubmitted;
    private boolean borrowedInputReleaseFailed;
    private volatile BorrowedInputReadiness borrowedReadiness;
    private boolean borrowedReadinessClaimed;
    private VkGlInteropSemaphore slotDepthReady, slotMotionReady;
    private final BorrowedSemaphoreCycle borrowedCycle = new BorrowedSemaphoreCycle();
    private java.util.concurrent.CompletableFuture<io.homo.superresolution.core.graphics.vulkan.GlReleaseContract.Submission> releaseSubmission = new java.util.concurrent.CompletableFuture<>();

    FrameResources(
            int index,
            VulkanDevice device,
            FramePacingTiming framePacingTiming
    ) {
        if (framePacingTiming == null) {
            throw new IllegalArgumentException("framePacingTiming cannot be null");
        }
        this.index = index;
        this.device = device;
        this.framePacingTiming = framePacingTiming;
        finalColor = new FrameTextureResource(device, "SRPresentationFinalColor-" + index);
        hudlessColor = new FrameTextureResource(device, "SRPresentationHudlessColor-" + index);
        depth = new FrameTextureResource(device, "SRPresentationDepth-" + index);
        motionVector = new FrameTextureResource(device, "SRPresentationMotionVector-" + index);
    }

    void begin(long newGeneration, int newLogicalFrameIndex) {
        if (unrecoverable) {
            throw new IllegalStateException("Frame capture slot cannot be reused after ownership recovery failed");
        }
        awaitReusable();
        generation = newGeneration;
        logicalFrameIndex = newLogicalFrameIndex;
        metadata = null;
        finalColor.beginFrame();
        hudlessColor.beginFrame();
        depth.beginFrame();
        motionVector.beginFrame();
        submittedCommandBuffer = null;
        submittedCommandBufferGeneration = 0L;
        fence = 0L;
        clearDlssGInputCompletion();
        resetBorrowedInputReleaseSubmission();
        borrowedReadiness = null;
        borrowedReadinessClaimed = false;
        releaseSubmission = new java.util.concurrent.CompletableFuture<>();
        lifecycle.beginRecording();
        traceLifecycle();
    }

    void copyFinalColor(ITexture source) {
        requireWritable();
        finalColor.copyFrom(source, false);
    }

    void copyHudlessColor(ITexture source) {
        requireWritable();
        hudlessColor.copyFrom(source, false);
    }

    void copyDepth(ITexture source) {
        requireWritable();
        depth.copyFrom(source, false);
    }

    void copyMotionVector(ITexture source) {
        requireWritable();
        motionVector.copyFrom(source, true);
    }

    void borrowDepth(
            VulkanTexture vkTexture,
            GlImportableTexture2D glTexture,
            VkGlInteropSemaphore ready,
            VkGlInteropSemaphore release
    ) {
        requireWritable();
        if (slotDepthReady == null) slotDepthReady = VkGlInteropSemaphore.create(device);
        depth.borrow(vkTexture, glTexture, slotDepthReady, release);
        if (depth.isValid()) {
            requireBorrowedInputReleaseSubmission();
        }
    }

    void borrowMotionVector(
            VulkanTexture vkTexture,
            GlImportableTexture2D glTexture,
            VkGlInteropSemaphore ready,
            VkGlInteropSemaphore release
    ) {
        requireWritable();
        if (slotMotionReady == null) slotMotionReady = VkGlInteropSemaphore.create(device);
        motionVector.borrow(vkTexture, glTexture, slotMotionReady, release);
        if (motionVector.isValid()) {
            requireBorrowedInputReleaseSubmission();
        }
    }

    void seal() {
        metadata = io.homo.superresolution.common.framegeneration.constants.FGConstantsFeature.getMetadata(logicalFrameIndex);
        if (metadata != null && finalColor.isValid()) {
            metadata = metadata.withColor(new io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata.Color(
                    finalColor.vkTexture().getTextureFormat().name(), irisOutputColorSpace(), null, null,
                    "Iris output color space; HDR luminance unavailable"));
            if (Boolean.getBoolean("sr.baselineDiagnostics") && metadata.monotonicFrameId() <= 5) {
                io.homo.superresolution.common.SuperResolution.LOGGER.info("SR_FG_INPUT sealed metadata={} final={} hudless={} depth={} mv={}",
                        metadata, hasFinalColor(), hasHudlessColor(), hasDepth(), hasMotionVector());
            }
        }
        lifecycle.seal();
        traceLifecycle();
    }

    void discardEmptyRecording() {
        if (hasAnyResource()) {
            throw new IllegalStateException("Cannot discard a capture slot that owns resources");
        }
        lifecycle.discardEmptyRecording();
    }

    public void markQueued() {
        lifecycle.markQueued();
        traceLifecycle();
    }

    public void markDispatching() {
        lifecycle.markDispatching();
        traceLifecycle();
    }

    public void markSubmitted(VulkanCommandBuffer commandBuffer, long submittedFence) {
        if (commandBuffer == null || submittedFence == 0L) {
            throw new IllegalArgumentException(
                    "Submitted command buffer and fence must be non-null/non-zero"
            );
        }
        lifecycle.requireSubmittable();
        long commandBufferGeneration = commandBuffer.submissionGeneration();
        if (commandBufferGeneration <= 0L) {
            throw new IllegalStateException(
                    "Frame capture cannot track a command buffer without a submission generation"
            );
        }
        submittedCommandBuffer = commandBuffer;
        submittedCommandBufferGeneration = commandBufferGeneration;
        fence = submittedFence;
        finalColor.markSubmitted();
        hudlessColor.markSubmitted();
        depth.markSubmitted();
        motionVector.markSubmitted();
        lifecycle.markSubmitted();
        if (borrowedReadiness != null) borrowedCycle.consume(generation);
        traceLifecycle();
        publishBorrowedInputReleaseSubmission();
    }

    public void markUnrecoverable() {
        unrecoverable = true;
        failBorrowedInputReleaseSubmission();
    }

    /**
     * Waits until a queue submission has published the release signals for borrowed
     * interop inputs. OpenGL must not wait on those semaphores before that submission exists.
     */
    public void awaitBorrowedInputReleaseSubmission() {
        synchronized (borrowedInputReleaseMonitor) {
            while (borrowedInputReleaseRequired
                    && !borrowedInputReleaseSubmitted
                    && !borrowedInputReleaseFailed) {
                long waitStartedAtNanos = System.nanoTime();
                try {
                    borrowedInputReleaseMonitor.wait();
                } catch (InterruptedException exception) {
                    Thread.currentThread().interrupt();
                    throw new IllegalStateException(
                            "Interrupted while waiting for borrowed interop input release submission",
                            exception
                    );
                } finally {
                    framePacingTiming.recordExcludedWait(
                            Math.max(0L, System.nanoTime() - waitStartedAtNanos)
                    );
                }
            }
            if (borrowedInputReleaseFailed) {
                throw new IllegalStateException(
                        "Borrowed interop input release could not be submitted"
                );
            }
        }
    }

    void destroy() {
        awaitReusable();
        finalColor.destroy();
        hudlessColor.destroy();
        depth.destroy();
        motionVector.destroy();
        if (slotDepthReady != null) { slotDepthReady.destroy(); slotDepthReady = null; }
        if (slotMotionReady != null) { slotMotionReady.destroy(); slotMotionReady = null; }
    }

    public int index() {
        return index;
    }

    public long generation() {
        return generation;
    }

    public int logicalFrameIndex() {
        return logicalFrameIndex;
    }
    void awaitSubmissionForReuse() { lifecycle.awaitSubmissionForReuse(() -> unrecoverable); }

    public io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata metadata() { return metadata; }

    private static String irisOutputColorSpace() {
        // Iris is optional. Read its declared output space rather than infer transfer from texture format.
        try {
            return String.valueOf(Class.forName("net.irisshaders.iris.gui.option.IrisVideoSettings")
                    .getField("colorSpace").get(null));
        } catch (ReflectiveOperationException | LinkageError exception) {
            return "UNKNOWN";
        }
    }

    public boolean hasFinalColor() {
        return finalColor.isValid();
    }

    public boolean hasHudlessColor() {
        return hudlessColor.isValid();
    }

    public boolean hasDepth() {
        return depth.isValid();
    }

    public boolean hasMotionVector() {
        return motionVector.isValid();
    }

    public boolean hasAnyResource() {
        return hasFinalColor() || hasHudlessColor() || hasDepth() || hasMotionVector();
    }

    public boolean isSealed() {
        return switch (lifecycle.state()) {
            case SEALED, QUEUED, DISPATCHING, SUBMITTED -> true;
            case REUSABLE, RECORDING -> false;
        };
    }

    public boolean isSubmitted() {
        return lifecycle.state() == FrameResourceState.SUBMITTED && fence != 0L;
    }

    public FrameResourceState state() {
        return lifecycle.state();
    }

    public VulkanTexture finalColorVulkanTexture() {
        return finalColor.vkTexture();
    }

    public VulkanTexture hudlessColorVulkanTexture() {
        return hudlessColor.vkTexture();
    }

    public VulkanTexture depthVulkanTexture() {
        return depth.vkTexture();
    }

    public VulkanTexture motionVectorVulkanTexture() {
        return motionVector.vkTexture();
    }

    /** Borrowed algorithm inputs obey its Y convention; owned capture inputs are always flipped. */
    public boolean hasBorrowedAlgorithmInputs() { return borrowedInputReleaseRequired; }

    public void publishBorrowedReadiness(BorrowedInputReadiness receipt) {
        requireWritable();
        if (!hasBorrowedAlgorithmInputs() || borrowedReadiness != null
                || receipt.captureGeneration() != generation || receipt.frame() != logicalFrameIndex)
            throw new IllegalStateException("Invalid borrowed readiness publication");
        borrowedReadiness = receipt;
    }

    public BorrowedInputReadiness claimBorrowedReadiness(VulkanDevice consumer) {
        BorrowedInputReadiness receipt = borrowedReadiness;
        if (receipt == null || borrowedReadinessClaimed || !isSealed() || unrecoverable)
            throw new IllegalStateException("Borrowed inputs require fresh submitted producer readiness");
        var main = consumer.getMainQueue(); var fg = consumer.requireFgQueue();
        var p = receipt.producer();
        receipt.validate(generation, logicalFrameIndex,
                new BorrowedInputReadiness.Submission(consumer.getVkDevice().address(), main.getQueue().address(),
                        main.getQueueFamilyIndex(), main.getQueueIndex(), p.command(), p.generation(), p.fence()),
                consumer.getVkDevice().address(), fg.getQueue().address(), fg.getQueueFamilyIndex(), fg.getQueueIndex(),
                sourceReceipt(depth), sourceReceipt(motionVector), readySemaphores());
        borrowedReadinessClaimed = true;
        return receipt;
    }

    private static BorrowedInputReadiness.Source sourceReceipt(FrameTextureResource resource) {
        var t = resource.vkTexture();
        if (!resource.isValid() || t == null) throw new IllegalStateException("Borrowed source lifetime ended");
        return new BorrowedInputReadiness.Source(t.handle(), t.getTextureFormat().vk(), t.getWidth(), t.getHeight(),
                t.getCurrentLayout(), resource.readySemaphore());
    }

    /** A sealed capture is already published to the independent consumer worker.
     * GL's server wait may precede its future Vulkan signal; no CPU rendezvous is needed.
     * Failure/retirement remains fail-closed; old producers retain their original path.
     */
    public boolean hasGpuOrderedBorrowedRelease() {
        if (borrowedReadiness == null) return false;
        if (unrecoverable || !isSealed()) throw new IllegalStateException("Borrowed capture has no live release path");
        return true;
    }

    public GlImportableTexture2D finalColorGlTexture() {
        return finalColor.glTexture();
    }

    public GlImportableTexture2D hudlessColorGlTexture() {
        return hudlessColor.glTexture();
    }

    public GlImportableTexture2D depthGlTexture() {
        return depth.glTexture();
    }

    public GlImportableTexture2D motionVectorGlTexture() {
        return motionVector.glTexture();
    }

    // Called on the present path for every frame, so these fill a right-sized primitive
    // array directly instead of boxing through a List<Long> and a Stream.
    public long[] readySemaphores() {
        long[] semaphores = new long[validResourceCount()];
        int count = 0;
        count = addReadySemaphore(semaphores, count, finalColor);
        count = addReadySemaphore(semaphores, count, hudlessColor);
        count = addReadySemaphore(semaphores, count, depth);
        addReadySemaphore(semaphores, count, motionVector);
        return semaphores;
    }

    public long[] scheduleBorrowedReadySignals() {
        requireWritable();
        if (!depth.isValid() || !motionVector.isValid() || !hasBorrowedAlgorithmInputs())
            throw new IllegalStateException("Borrowed ready pair requires both live inputs");
        borrowedCycle.signal(generation);
        return new long[]{depth.readySemaphore(), motionVector.readySemaphore()};
    }

    public long borrowedDepthReady() { return depth.readySemaphore(); }
    public long borrowedMotionReady() { return motionVector.readySemaphore(); }

    public long[] releaseSemaphores() {
        long[] semaphores = new long[validResourceCount()];
        int count = 0;
        count = addReleaseSemaphore(semaphores, count, finalColor);
        count = addReleaseSemaphore(semaphores, count, hudlessColor);
        count = addReleaseSemaphore(semaphores, count, depth);
        addReleaseSemaphore(semaphores, count, motionVector);
        return semaphores;
    }

    private int validResourceCount() {
        int count = 0;
        if (finalColor.isValid()) {
            count++;
        }
        if (hudlessColor.isValid()) {
            count++;
        }
        if (depth.isValid()) {
            count++;
        }
        if (motionVector.isValid()) {
            count++;
        }
        return count;
    }

    private void awaitReusable() {
        FrameResourceState state = lifecycle.state();
        if (state == FrameResourceState.REUSABLE) {
            return;
        }
        if (state != FrameResourceState.SUBMITTED) {
            throw new IllegalStateException(
                    "Frame capture slot is still owned in state " + state
            );
        }
        VulkanCommandBuffer commandBuffer = submittedCommandBuffer;
        long commandBufferGeneration = submittedCommandBufferGeneration;
        if (commandBuffer != null && commandBufferGeneration > 0L) {
            commandBuffer.waitForSubmission(commandBufferGeneration);
        }
        if (borrowedReadiness != null) {
            if (commandBuffer == null || commandBufferGeneration <= 0)
                throw new IllegalStateException("Borrowed retirement has no tracked output fence");
            borrowedCycle.retire(generation, commandBuffer.isSubmissionComplete(commandBufferGeneration));
        }
        // The tracked Vulkan output submission waits on this D3D12 completion timeline.
        // Its completed fence already covers it; do not add a cross-API host wait.
        if (borrowedReadiness != null) clearDlssGInputCompletion();
        else awaitDlssGInputs();
        finalColor.awaitOwnedRelease();
        hudlessColor.awaitOwnedRelease();
        depth.awaitOwnedRelease();
        motionVector.awaitOwnedRelease();
        submittedCommandBuffer = null;
        submittedCommandBufferGeneration = 0L;
        fence = 0L;
        lifecycle.markReusable();
        traceLifecycle();
    }

    private void traceLifecycle() {
        if (Boolean.getBoolean("sr.baselineDiagnostics") && index == 0 && generation <= 12) {
            io.homo.superresolution.common.SuperResolution.LOGGER.info(
                    "SR_INTEROP slot={} generation={} frame={} state={} fence={} ready={} release={}",
                    index, generation, logicalFrameIndex, lifecycle.state(), fence,
                    java.util.Arrays.toString(readySemaphores()),
                    java.util.Arrays.toString(releaseSemaphores()));
        }
    }
    public void setDlssGInputCompletion(long semaphore, long value) {
        if (!isSubmitted() || semaphore == 0L || value <= 0L) {
            throw new IllegalArgumentException("Invalid DLSS-G input completion timeline");
        }
        dlssGInputCompletionSemaphore = semaphore;
        dlssGInputCompletionValue = value;
    }

    public void clearDlssGInputCompletion() {
        dlssGInputCompletionSemaphore = 0L;
        dlssGInputCompletionValue = 0L;
    }
    private void requireWritable() {
        if (lifecycle.state() != FrameResourceState.RECORDING) {
            throw new IllegalStateException(
                    "Frame capture is not writable in state " + lifecycle.state()
            );
        }
    }
    private void awaitDlssGInputs() {
        if (dlssGInputCompletionSemaphore == 0L) {
            return;
        }
        try (MemoryStack stack = MemoryStack.stackPush()) {
            VkSemaphoreWaitInfo waitInfo = VkSemaphoreWaitInfo.calloc(stack)
                    .sType(VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO)
                    .semaphoreCount(1)
                    .pSemaphores(stack.longs(dlssGInputCompletionSemaphore))
                    .pValues(stack.longs(dlssGInputCompletionValue));
            int result = vkWaitSemaphores(device.getVkDevice(), waitInfo, Long.MAX_VALUE);
            if (result != VK_SUCCESS) {
                throw new IllegalStateException(
                        "Failed to wait for DLSS-G input completion, VkResult=" + result
                );
            }
        }
        clearDlssGInputCompletion();
    }

    private void resetBorrowedInputReleaseSubmission() {
        synchronized (borrowedInputReleaseMonitor) {
            borrowedInputReleaseRequired = false;
            borrowedInputReleaseSubmitted = false;
            borrowedInputReleaseFailed = false;
        }
    }

    private void requireBorrowedInputReleaseSubmission() {
        synchronized (borrowedInputReleaseMonitor) {
            borrowedInputReleaseRequired = true;
        }
    }

    private void publishBorrowedInputReleaseSubmission() {
        synchronized (borrowedInputReleaseMonitor) {
            if (borrowedInputReleaseRequired) {
                borrowedInputReleaseSubmitted = true;
                borrowedInputReleaseMonitor.notifyAll();
                releaseSubmission.complete(new io.homo.superresolution.core.graphics.vulkan.GlReleaseContract.Submission(
                        index,generation,logicalFrameIndex,metadata==null?0:metadata.monotonicFrameId(),
                        submittedCommandBuffer.getNativeCommandBuffer().address(),fence,depth.releaseSemaphore(),motionVector.releaseSemaphore()));
            }
        }
    }

    private void failBorrowedInputReleaseSubmission() {
        synchronized (borrowedInputReleaseMonitor) {
            if (borrowedInputReleaseRequired) {
                borrowedInputReleaseFailed = true;
                borrowedInputReleaseMonitor.notifyAll();
                releaseSubmission.completeExceptionally(new IllegalStateException("Borrowed release submission failed"));
            }
        }
    }

    public java.util.concurrent.CompletableFuture<io.homo.superresolution.core.graphics.vulkan.GlReleaseContract.Submission> releaseSubmissionNotification(){return releaseSubmission;}

    private static int addReadySemaphore(long[] semaphores, int count, FrameTextureResource resource) {
        if (!resource.isValid()) {
            return count;
        }
        semaphores[count] = resource.readySemaphore();
        return count + 1;
    }

    private static int addReleaseSemaphore(long[] semaphores, int count, FrameTextureResource resource) {
        if (!resource.isValid()) {
            return count;
        }
        semaphores[count] = resource.releaseSemaphore();
        return count + 1;
    }
}
