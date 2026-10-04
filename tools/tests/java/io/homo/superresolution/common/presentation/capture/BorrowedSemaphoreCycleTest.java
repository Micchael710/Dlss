package io.homo.superresolution.common.presentation.capture;
import io.homo.superresolution.core.graphics.vulkan.PendingGpuReuse;
public final class BorrowedSemaphoreCycleTest {
    private static int rejected;
    private static void rejects(Runnable r) {
        try { r.run(); } catch (IllegalStateException expected) { rejected++; return; }
        throw new AssertionError("Unsafe reuse accepted");
    }
    public static void main(String[] args) {
        var cycle = new BorrowedSemaphoreCycle();
        cycle.signal(1);
        rejects(() -> cycle.signal(1)); // binary signaled twice without consume
        rejects(() -> cycle.signal(2)); // next interval before retirement
        rejects(() -> cycle.consume(2)); // stale readiness
        cycle.consume(1);
        rejects(() -> cycle.consume(1)); // duplicate wait
        rejects(() -> cycle.signal(2)); // consume submitted is not completion
        rejects(() -> cycle.retire(1, false)); // output fence still pending
        cycle.retire(1, true); cycle.signal(2); cycle.consume(2); cycle.retire(2, true);
        rejects(() -> PendingGpuReuse.requireRetired(true, false)); // fence reset before completion
        rejects(() -> PendingGpuReuse.requireRetired(true, false)); // command buffer reset before retirement
        PendingGpuReuse.requireRetired(true, true); PendingGpuReuse.requireRetired(false, false);
        System.out.println("PASS reuse CPU contracts: " + rejected + " unsafe cases rejected; no GPU results simulated");
    }
}
