package io.homo.superresolution.common.presentation.vulkan;

import org.junit.Test;
import java.util.concurrent.*;
import static org.junit.Assert.*;

public class FrameQueueReadinessTest {
    @Test public void pendingHeadKeepsFifoAndWakesOnCompletionEvenAfterClose() throws Exception {
        var queue=new FrameQueue<CompletableFuture<Void>>(3);
        var pending=new CompletableFuture<Void>();
        var later=CompletableFuture.<Void>completedFuture(null);
        queue.put(pending);queue.put(later);
        pending.whenComplete((value,error)->queue.signalConsumer());
        var executor=Executors.newSingleThreadExecutor();
        try {
            Future<FrameQueue.HeadResult<CompletableFuture<Void>>> head=executor.submit(()->queue.awaitReadyHead(CompletableFuture::isDone));
            try {head.get(40,TimeUnit.MILLISECONDS);fail("Pending head must not be bypassed");}
            catch(TimeoutException expected) {}
            queue.close();pending.complete(null);
            assertSame(pending,head.get(1,TimeUnit.SECONDS).value());
            queue.removeHead(pending);
            assertSame(later,queue.awaitReadyHead(CompletableFuture::isDone).value());
            queue.removeHead(later);
            assertTrue(queue.awaitReadyHead(CompletableFuture::isDone).closedAndEmpty());
        } finally {executor.shutdownNow();}
    }
    @Test public void exceptionalCompletionWakesConsumerForTerminalCleanup() throws Exception {
        var queue=new FrameQueue<CompletableFuture<Void>>(1);var failed=new CompletableFuture<Void>();
        queue.put(failed);failed.whenComplete((v,e)->queue.signalConsumer());
        failed.completeExceptionally(new IllegalStateException("device removed"));
        assertSame(failed,queue.awaitReadyHead(CompletableFuture::isDone).value());
        assertTrue(failed.isCompletedExceptionally());
        queue.removeHead(failed);queue.close();
    }
}
