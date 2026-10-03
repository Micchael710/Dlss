package org.ireallywanttosleep.wisteria.fsr;

/** Worker-owned native context. No swapchain handle is accepted by this ABI. */
public final class FsrSession implements AutoCloseable {
    private long handle;
    public FsrSession(long handle) { if (handle == 0) throw new IllegalArgumentException("Null context"); this.handle = handle; }
    public long handle() { if (handle == 0) throw new IllegalStateException("Closed context"); return handle; }
    public void close() { if (handle != 0) { FidelityFxBridge.destroySession(handle); handle = 0; } }
}
