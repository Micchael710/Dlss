package io.homo.superresolution.core.graphics.vulkan;

/** Pure guard; its caller obtains completion from Vulkan, never from this predicate. */
public final class PendingGpuReuse {
    private PendingGpuReuse() {}
    public static void requireRetired(boolean pending, boolean completed) {
        if (pending && !completed) throw new IllegalStateException("GPU object still pending; reset/reuse rejected");
    }
}
