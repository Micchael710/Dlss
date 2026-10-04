package io.homo.superresolution.common.presentation.capture;

/** Per-capture-slot binary readiness lifetime. Completion comes from the tracked GPU fence. */
public final class BorrowedSemaphoreCycle {
    private long generation;
    private boolean consumed;
    public synchronized void signal(long next) {
        if (next <= 0 || generation != 0) throw new IllegalStateException("Binary readiness still pending or signaled twice");
        generation = next; consumed = false;
    }
    public synchronized void consume(long current) {
        if (current != generation || generation == 0 || consumed)
            throw new IllegalStateException("Stale or duplicate binary readiness wait");
        consumed = true;
    }
    public synchronized void retire(long current, boolean outputFenceComplete) {
        if (current != generation || !consumed || !outputFenceComplete)
            throw new IllegalStateException("Readiness reused before consuming GPU submission completed");
        generation = 0;
    }
}
