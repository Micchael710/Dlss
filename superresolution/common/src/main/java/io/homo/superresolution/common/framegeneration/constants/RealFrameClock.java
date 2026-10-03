package io.homo.superresolution.common.framegeneration.constants;

import io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata;
import java.util.function.LongSupplier;

/** Render-thread clock; measures wall intervals between real dispatches, never tick/pacer time. */
public final class RealFrameClock {
    private final LongSupplier clock;
    private long previousNanos, frameId, epoch;
    private int previousLogicalId;
    private RealFrameMetadata previous;
    private boolean invalidated;
    public RealFrameClock(LongSupplier clock) { this.clock = java.util.Objects.requireNonNull(clock); }
    public RealFrameMetadata capture(int logicalId, RealFrameMetadata.Size render,
                                     RealFrameMetadata.Size display, boolean reset) {
        if (previous != null && logicalId == previousLogicalId && !invalidated) return previous;
        long now = clock.getAsLong();
        boolean discontinuity = previous == null || invalidated || reset || logicalId != previousLogicalId + 1
                || !render.equals(previous.renderSize()) || !display.equals(previous.displaySize());
        if (discontinuity) epoch++;
        double delta = previous == null ? 0.0 : Math.max(0L, now - previousNanos) / 1_000_000.0;
        previous = new RealFrameMetadata(now, delta, ++frameId, epoch, render, display,
                RealFrameMetadata.Color.unknown(), Double.NaN);
        previousNanos = now;
        previousLogicalId = logicalId;
        invalidated = false;
        return previous;
    }
    public void invalidateHistory() { invalidated = true; }
}
