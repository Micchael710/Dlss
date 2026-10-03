package io.homo.superresolution.common.framegeneration.constants;

import io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata;
import org.junit.Test;
import java.util.concurrent.atomic.AtomicLong;
import static org.junit.Assert.*;

public class RealFrameClockTest {
    @Test public void realIntervalsAreMillisecondsAndSnapshotsRemainIndependent() {
        AtomicLong now = new AtomicLong(1_000_000_000L);
        RealFrameClock clock = new RealFrameClock(now::get);
        var size = new RealFrameMetadata.Size(854, 480);
        var first = clock.capture(10, size, size, true);
        now.addAndGet(16_500_000L);
        var second = clock.capture(11, size, size, false);
        assertEquals(16.5, second.realFrameDeltaMs(), 0.00001);
        assertEquals(first.monotonicFrameId() + 1, second.monotonicFrameId());
        assertEquals(first.discontinuityEpoch(), second.discontinuityEpoch());
        assertSame(second, clock.capture(11, size, size, false));
        var recolored = second.withColor(new RealFrameMetadata.Color("RGBA8", "SRGB", null, null, "test"));
        assertEquals("UNKNOWN", second.color().format());
        assertEquals("RGBA8", recolored.color().format());
        assertEquals(0.0, first.realFrameDeltaMs(), 0.0);
    }
    @Test public void resetsGapsAndResizeAdvanceEpochWithoutResettingId() {
        AtomicLong now = new AtomicLong(1);
        RealFrameClock clock = new RealFrameClock(now::get);
        var size = new RealFrameMetadata.Size(100, 100);
        var a = clock.capture(1, size, size, false);
        var b = clock.capture(2, size, size, true);
        var c = clock.capture(5, size, size, false);
        var d = clock.capture(6, size, new RealFrameMetadata.Size(200, 100), false);
        assertEquals(a.discontinuityEpoch() + 1, b.discontinuityEpoch());
        assertEquals(b.discontinuityEpoch() + 1, c.discontinuityEpoch());
        assertEquals(c.discontinuityEpoch() + 1, d.discontinuityEpoch());
        assertEquals(4, d.monotonicFrameId());
        clock.invalidateHistory();
        var e = clock.capture(6, size, size, false);
        assertEquals(5, e.monotonicFrameId());
        assertEquals(d.discontinuityEpoch() + 1, e.discontinuityEpoch());
    }
}
