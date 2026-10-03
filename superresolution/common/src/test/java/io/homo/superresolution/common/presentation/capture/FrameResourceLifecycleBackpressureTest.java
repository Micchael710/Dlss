package io.homo.superresolution.common.presentation.capture;

import org.junit.Test;
import java.util.concurrent.*;
import static org.junit.Assert.*;

public class FrameResourceLifecycleBackpressureTest {
    @Test public void producerCannotReuseDispatchingSlotBeforeSubmission() throws Exception {
        var lifecycle = new FrameResourceLifecycle();
        lifecycle.beginRecording(); lifecycle.seal(); lifecycle.markQueued(); lifecycle.markDispatching();
        var executor = Executors.newSingleThreadExecutor();
        try {
            var entered = new CountDownLatch(1);
            var wait = executor.submit(() -> { entered.countDown(); lifecycle.awaitSubmissionForReuse(() -> false); });
            assertTrue(entered.await(1, TimeUnit.SECONDS));
            try { wait.get(30, TimeUnit.MILLISECONDS); fail("Still-owned slot became reusable"); }
            catch (TimeoutException expected) { assertEquals(FrameResourceState.DISPATCHING, lifecycle.state()); }
            lifecycle.markSubmitted(); wait.get(1, TimeUnit.SECONDS);
            assertEquals(FrameResourceState.SUBMITTED, lifecycle.state());
            lifecycle.markReusable(); lifecycle.beginRecording();
        } finally { executor.shutdownNow(); }
    }
    @Test public void failedOwnershipIsNotConvertedIntoReuse() {
        var lifecycle = new FrameResourceLifecycle();
        lifecycle.beginRecording(); lifecycle.seal(); lifecycle.markQueued();
        try { lifecycle.awaitSubmissionForReuse(() -> true); fail("Failed slot reused"); }
        catch (IllegalStateException expected) { assertEquals(FrameResourceState.QUEUED, lifecycle.state()); }
    }
}
