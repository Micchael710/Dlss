/*
 * Super Resolution
 * Copyright (c) 2026. 187J3X1-114514
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

package io.homo.superresolution.common.presentation.vulkan;

import io.homo.superresolution.api.registry.framegeneration.ProviderInputSnapshot;
import io.homo.superresolution.common.framegeneration.FrameGeneration;
import io.homo.superresolution.common.lowlatency.LowLatency;
import io.homo.superresolution.common.perf.FramePacingTrace;
import io.homo.superresolution.common.presentation.capture.CaptureFrameRing;
import io.homo.superresolution.common.presentation.capture.FrameResources;
import io.homo.superresolution.core.graphics.vulkan.VulkanDevice;
import io.homo.superresolution.core.graphics.vulkan.VulkanLowLatency;

import java.util.concurrent.atomic.AtomicLong;
import java.util.function.BooleanSupplier;

public final class AsyncFramePresenter implements AutoCloseable {
    static final int GENERATION_QUEUE_CAPACITY = CaptureFrameRing.MAX_IN_FLIGHT_FRAMES - 1;
    static final int MAX_GENERATED_FRAMES = 5;
    static final int MAX_PRESENTS_PER_BATCH = MAX_GENERATED_FRAMES + 1;
    static final int PRESENTATION_QUEUE_CAPACITY = MAX_PRESENTS_PER_BATCH;
    /** Queue image budget, one present-in-flight batch and one dispatch-in-flight batch. */
    public static int maximumLiveProviderLeases(int generatedCount) {
        if (generatedCount < 0 || generatedCount > MAX_GENERATED_FRAMES)
            throw new IllegalArgumentException("Invalid generated count");
        return PRESENTATION_QUEUE_CAPACITY / (generatedCount + 1) + 2;
    }
    public static final int MAX_PIPELINED_REAL_FRAMES_X4 = 2;
    private static final long THREAD_JOIN_TIMEOUT_NANOS = 2_000_000_000L;

    final Object stateLock = new Object();
    final java.util.concurrent.atomic.AtomicInteger activeRealFrames =
            new java.util.concurrent.atomic.AtomicInteger();
    final FrameQueue<FrameGenerationWork> generationQueue =
            new FrameQueue<>(GENERATION_QUEUE_CAPACITY);
    final FrameQueue<PresentImageBatch> presentationQueue =
            new FrameQueue<>(PRESENTATION_QUEUE_CAPACITY, PresentImageBatch::imageCount);
    final PresentPacer pacer;
    private final String providerId;
    private final AtomicLong nextRealIndex = new AtomicLong();
    private final FramePacingTiming framePacingTiming;
    private final FrameGenerationWorker generationWorker;
    private final PresentWorker presentWorker;
    private long nextBatchId;
    private volatile Throwable failure;
    private Runnable terminalTeardown;

    AsyncFramePresenter(
            String providerId, FramePacingTiming timing, PresentPacer pacer
    ) {
        this.providerId = providerId;
        this.framePacingTiming = timing;
        this.pacer = pacer;
        this.generationWorker = null;
        this.presentWorker = null;
    }

    AsyncFramePresenter(VulkanSwapchain swapchain, VulkanDevice device, String providerId) {
        this(swapchain, device, providerId, swapchain.framePacingTiming(),
                swapchain.presentPacer(), System::nanoTime);
    }

    AsyncFramePresenter(
            VulkanSwapchain swapchain, VulkanDevice device, String providerId, NanoClock clock
    ) {
        this(swapchain, device, providerId, new FramePacingTiming(clock::nanoTime), clock);
    }

    private AsyncFramePresenter(
            VulkanSwapchain swapchain, VulkanDevice device, String providerId,
            FramePacingTiming timing, NanoClock clock
    ) {
        this(swapchain, device, providerId, timing, new PresentPacer(clock, timing), clock);
    }

    private AsyncFramePresenter(
            VulkanSwapchain swapchain, VulkanDevice device, String providerId,
            FramePacingTiming timing, PresentPacer pacer, NanoClock clock
    ) {
        if (!device.asyncDispatchCapabilities().available()
                || !providerId.equals(device.asyncDispatchCapabilities().providerId())) {
            throw new IllegalStateException("The presenter requires the selected provider's async queues");
        }
        this.providerId = providerId;
        this.framePacingTiming = timing;
        this.pacer = pacer;
        this.generationWorker = new FrameGenerationWorker(this, swapchain, device, providerId);
        this.presentWorker = new PresentWorker(this, swapchain, device);
        presentWorker.start();
        generationWorker.start();
    }

    String providerId() {
        return providerId;
    }

    boolean enqueue(FrameResources resources, boolean presentAllowed) {
        throwIfFailed();
        try {
            if (!FrameGeneration.isApplicationManagedPresenterCompatible(providerId)) {
                throw new IllegalStateException("Drain the presenter before switching frame-generation providers");
            }
        } catch (NoClassDefFoundError ignored) {
        }
        long realIndex = nextRealIndex.getAndIncrement();
        ProviderInputSnapshot snapshot = null;
        if (presentAllowed) {
            FramePacingTrace.Span snapshotTrace = FramePacingTrace.INSTANCE.begin(
                    "frame_generation_input_snapshot",
                    resources.logicalFrameIndex(),
                    realIndex,
                    -1L,
                    -1L,
                    -1L,
                    "REAL",
                    providerId
            );
            try {
                snapshot = FrameGeneration.captureProviderInputSnapshotForFrame(providerId, resources);
                if (snapshot != null && (!providerId.equals(snapshot.providerId())
                        || snapshot.logicalFrameIndex() != resources.logicalFrameIndex())) {
                    logWarn("Provider '{}' returned mismatched input for real frame {}",
                            providerId, realIndex);
                    snapshot = null;
                }
            } catch (Throwable throwable) {
                logWarn("Failed to capture provider '{}' input; using real-only fallback",
                        providerId, throwable);
            } finally {
                snapshotTrace.close();
            }
        }
        long latencyFrameId = 0L;
        try {
            latencyFrameId = LowLatency.currentLatencyFrameId();
        } catch (NoClassDefFoundError ignored) {
        }
        long presentId = 0L;
        if (presentAllowed) {
            try {
                presentId = VulkanLowLatency.claimCurrentFramePresentId();
            } catch (NoClassDefFoundError ignored) {
            }
        }
        int plannedCount = MAX_GENERATED_FRAMES;
        try {
            plannedCount = Math.min(Math.max(0, FrameGeneration.plannedGeneratedFrameCount()), MAX_GENERATED_FRAMES);
        } catch (NoClassDefFoundError ignored) {
        }
        FrameGenerationWork work = new FrameGenerationWork(
                realIndex, resources.logicalFrameIndex(), latencyFrameId,
                presentId,
                resources, snapshot, pacer.takeRealFrameProducerTimeNanos(),
                plannedCount,
                snapshot != null && snapshot.historyResetRequested(), presentAllowed
        );
        resources.markQueued();
        FramePacingTrace.Span enqueueTrace = FramePacingTrace.INSTANCE.begin(
                "frame_generation_enqueue_wait",
                work.logicalFrameIndex(),
                work.realIndex(),
                -1L,
                -1L,
                work.realPresentId(),
                "REAL",
                providerId
        );
        try {
            long waitNs = generationQueue.put(work);
            activeRealFrames.incrementAndGet();
            framePacingTiming.recordExcludedWait(waitNs);
            if ("wisteria:dlssg_fg".equals(providerId) && (waitNs > 0 || work.realIndex() % 60 == 0 || Boolean.getBoolean("sr.debug"))) {
                logInfo(
                        "X4_ENQUEUE_TIMING realFrameId={} activeRealFrames={} generationQueueDepth={} generationQueueWaitNs={}",
                        work.realIndex(), activeRealFrames.get(), generationQueue.size(), waitNs);
            }
            return true;
        } catch (InterruptedException exception) {
            resources.markUnrecoverable();
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Interrupted while enqueueing a real frame", exception);
        } catch (IllegalStateException exception) {
            resources.markUnrecoverable();
            throwIfFailed();
            throw exception;
        } finally {
            enqueueTrace.close();
        }
    }

    long nextBatchId() {
        return ++nextBatchId;
    }

    void awaitPresentationDrain() {
        synchronized (stateLock) {
            generationWorker.pause();
            presentWorker.pause();
            awaitState(() -> !generationWorker.isInFlight()
                    && presentationQueue.isEmpty() && !presentWorker.isInFlight());
        }
        throwIfFailed();
    }

    void awaitPresentIdle() {
        presentWorker.awaitIdle();
    }

    void resumePresenting() {
        synchronized (stateLock) {
            if (generationWorker != null) {
                generationWorker.resume();
            }
            if (presentWorker != null) {
                presentWorker.resume();
            }
        }
    }

    void shutdownProviderOnFrameGenerationThread(String expectedProviderId, Runnable teardown) {
        synchronized (stateLock) {
            if (!providerId.equals(expectedProviderId) || (generationWorker != null && generationWorker.isTerminated())
                    || terminalTeardown != null) {
                throw new IllegalStateException("Provider teardown does not match the live presenter lifecycle");
            }
            terminalTeardown = teardown;
            generationQueue.close();
            resumePresenting();
        }
        if (generationWorker != null) {
            join(generationWorker.thread(), () -> {
            });
        }
        throwIfFailed();
    }

    @Override
    public void close() {
        generationQueue.close();
        resumePresenting();
        if (generationWorker != null) {
            join(generationWorker.thread(), () -> {
            });
        }
        if (presentWorker != null) {
            join(presentWorker.thread(), () -> {
            });
        }
        throwIfFailed();
    }

    void awaitPresentTermination(Runnable releasePending) {
        if (presentWorker != null) {
            join(presentWorker.thread(), releasePending);
        }
    }

    boolean isTerminated() {
        return (generationWorker == null || generationWorker.isTerminated())
                && (presentWorker == null || presentWorker.isTerminated());
    }

    void runTerminalTeardown() {
        Runnable teardown;
        synchronized (stateLock) {
            teardown = terminalTeardown;
        }
        if (teardown != null) {
            teardown.run();
        }
    }

    boolean hasFailed() {
        return failure != null;
    }

    void throwIfFailed() {
        if (failure != null) {
            throw new IllegalStateException("Application-managed frame presenter failed", failure);
        }
    }

    void fail(Throwable throwable) {
        synchronized (stateLock) {
            if (failure == null) {
                failure = throwable;
                logWarn("Application-managed frame presenter stopped", throwable);
            }
            generationQueue.close();
            presentationQueue.close();
            resumePresenting();
            stateLock.notifyAll();
        }
    }

    void retireBatch(FrameGenerationWorker.RetiredBatch batch) {
        if (generationWorker != null && batch != null) {
            generationWorker.retireBatch(batch);
        }
        activeRealFrames.updateAndGet(count -> Math.max(0, count - 1));
        synchronized (stateLock) {
            stateLock.notifyAll();
        }
    }

    public int activeRealFrames() {
        return activeRealFrames.get();
    }

    public int pipelineDepth() {
        return activeRealFrames.get();
    }

    void awaitState(BooleanSupplier complete) {
        boolean interrupted = false;
        while (!complete.getAsBoolean()) {
            try {
                stateLock.wait();
            } catch (InterruptedException exception) {
                interrupted = true;
            }
        }
        if (interrupted) {
            Thread.currentThread().interrupt();
        }
    }

    private void join(Thread thread, Runnable maintenance) {
        boolean interrupted = false;
        long deadline = System.nanoTime() + THREAD_JOIN_TIMEOUT_NANOS;
        while (thread.isAlive() && System.nanoTime() < deadline) {
            try {
                thread.join(20L);
            } catch (InterruptedException exception) {
                interrupted = true;
            }
            maintenance.run();
        }
        if (interrupted) {
            Thread.currentThread().interrupt();
        }
        if (thread.isAlive()) {
            thread.interrupt();
            throw new IllegalStateException("Presenter worker did not terminate before teardown");
        }
    }

    @FunctionalInterface
    interface NanoClock {
        long nanoTime();
    }

    private static void logInfo(String format, Object... args) {
        try {
            Class<?> factory = Class.forName("org.slf4j.LoggerFactory");
            Object logger = factory.getMethod("getLogger", String.class).invoke(null, "SuperResolution/AsyncFramePresenter");
            logger.getClass().getMethod("info", String.class, Object[].class).invoke(logger, format, args);
            return;
        } catch (Throwable ignored) {
        }
        String msg = format;
        for (Object arg : args) {
            msg = msg.replaceFirst("\\{\\}", java.util.regex.Matcher.quoteReplacement(String.valueOf(arg)));
        }
        System.out.println("[AsyncFramePresenter] " + msg);
    }

    private static void logWarn(String format, Object... args) {
        try {
            Class<?> factory = Class.forName("org.slf4j.LoggerFactory");
            Object logger = factory.getMethod("getLogger", String.class).invoke(null, "SuperResolution/AsyncFramePresenter");
            logger.getClass().getMethod("warn", String.class, Object[].class).invoke(logger, format, args);
            return;
        } catch (Throwable ignored) {
        }
        String msg = format;
        for (Object arg : args) {
            msg = msg.replaceFirst("\\{\\}", java.util.regex.Matcher.quoteReplacement(String.valueOf(arg)));
        }
        System.err.println("[AsyncFramePresenter] [WARN] " + msg);
    }

    private static void logWarn(String message, Throwable throwable) {
        try {
            Class<?> factory = Class.forName("org.slf4j.LoggerFactory");
            Object logger = factory.getMethod("getLogger", String.class).invoke(null, "SuperResolution/AsyncFramePresenter");
            logger.getClass().getMethod("warn", String.class, Throwable.class).invoke(logger, message, throwable);
            return;
        } catch (Throwable ignored) {
        }
        System.err.println("[AsyncFramePresenter] [WARN] " + message + ": " + throwable);
    }
}
