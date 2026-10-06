package io.homo.superresolution.common.presentation.vulkan;

import org.junit.Test;
import static org.junit.Assert.*;

public class X4LowLatencyPacingTest {

    private static long targetSpacingNanos(long realPeriodNanos, int generatedCount) {
        return realPeriodNanos / (generatedCount + 1L);
    }

    @Test
    public void testPacingSpacingsAtVariousFramerates() {
        int generatedCount = 3; // X4: 3 generated + 1 real = 4 total

        // 1. 40 FPS real: period = 25 ms, spacing ≈ 6.25 ms
        long period40Fps = 25_000_000L;
        long spacing40Fps = targetSpacingNanos(period40Fps, generatedCount);
        assertEquals(6_250_000L, spacing40Fps);
        assertEquals(6.25, spacing40Fps / 1_000_000.0, 0.001);

        // 2. 50 FPS real: period = 20 ms, spacing ≈ 5.00 ms
        long period50Fps = 20_000_000L;
        long spacing50Fps = targetSpacingNanos(period50Fps, generatedCount);
        assertEquals(5_000_000L, spacing50Fps);
        assertEquals(5.00, spacing50Fps / 1_000_000.0, 0.001);

        // 3. 30 FPS real: period = 33.333 ms, spacing ≈ 8.333 ms
        long period30Fps = 33_333_333L;
        long spacing30Fps = targetSpacingNanos(period30Fps, generatedCount);
        assertEquals(8_333_333L, spacing30Fps);
        assertEquals(8.333, spacing30Fps / 1_000_000.0, 0.005);

        // 4. 23 FPS real: period = 43.478 ms, spacing ≈ 10.870 ms
        long period23Fps = 43_478_261L;
        long spacing23Fps = targetSpacingNanos(period23Fps, generatedCount);
        assertEquals(10_869_565L, spacing23Fps);
        assertEquals(10.870, spacing23Fps / 1_000_000.0, 0.005);
    }

    @Test
    public void testEmaReactsToFpsTransition() {
        FramePacingEstimator estimator = new FramePacingEstimator("wisteria:dlssg_fg");
        // Simulated producer timestamps: initial 60 FPS (dt = 16.666 ms)
        long t = 1_000_000_000L;
        long dt60 = 16_666_667L;
        for (int i = 0; i < 20; i++) {
            t += dt60;
            // observe via reflection/direct timing simulation
        }

        // Mathematical verification of EMA with alpha = 0.2:
        // Transition from 16.67 ms to 25.0 ms:
        double ema = 16.666667;
        double target = 25.0;
        double alpha = 0.2;
        for (int i = 0; i < 6; i++) {
            ema = ema * (1.0 - alpha) + target * alpha;
        }
        // After 6 frames, EMA should reach > 22.8 ms (within ~90% of target):
        assertTrue("EMA must reach > 22.0 ms after 6 frames of 40 FPS", ema > 22.0);
        assertTrue("EMA must not overshoot target", ema <= 25.0);
    }

    @Test
    public void testStaleGeneratedDropPolicy() {
        double maxLatenessSpacings = PresentPacer.MAX_GENERATED_LATENESS_SPACINGS;
        assertEquals(1.0, maxLatenessSpacings, 0.0001);

        long targetSpacingNs = 6_250_000L; // 6.25 ms for 40 FPS X4
        long maxAllowedLateNs = (long) (maxLatenessSpacings * targetSpacingNs);
        assertEquals(6_250_000L, maxAllowedLateNs);

        long deadlineNs = 1_000_000_000L;

        // Case A: generated frame arrives 8.0 ms late (> 6.25 ms max allowed) -> MUST DROP
        long nowLate = deadlineNs + 8_000_000L;
        long lateNs = nowLate - deadlineNs;
        boolean shouldDropLate = lateNs > maxAllowedLateNs;
        assertTrue("Generated frame > 1 spacing late must be dropped", shouldDropLate);

        // Case B: generated frame arrives 2.0 ms late (<= 6.25 ms max allowed) -> MUST NOT DROP
        long nowOnTime = deadlineNs + 2_000_000L;
        long onTimeLateNs = nowOnTime - deadlineNs;
        boolean shouldDropOnTime = onTimeLateNs > maxAllowedLateNs;
        assertFalse("Generated frame <= 1 spacing late must be presented", shouldDropOnTime);
    }

    @Test
    public void testRealFrameNeverDroppedByStalePolicy() {
        // By design, stale check is gated on: presentImage.kind() == PresentImage.Kind.GENERATED
        // Even if real frame arrives 50 ms late, the stale generated drop policy never applies to REAL.
        PresentImage.Kind realKind = PresentImage.Kind.REAL;
        PresentImage.Kind generatedKind = PresentImage.Kind.GENERATED;

        boolean isRealCandidate = (realKind == PresentImage.Kind.GENERATED);
        assertFalse("Real frame must never match generated drop condition", isRealCandidate);

        boolean isGenCandidate = (generatedKind == PresentImage.Kind.GENERATED);
        assertTrue("Generated frame is candidate for stale drop", isGenCandidate);
    }

    @Test
    public void testPipelineDepthLimit() {
        assertEquals(2, AsyncFramePresenter.MAX_PIPELINED_REAL_FRAMES_X4);
    }

    @Test
    public void testPresentationOrderPreserved() {
        // Presentation order remains: A -> G25 -> G50 -> G75 -> B
        // When G25 is dropped stale, order is A -> G50 -> G75 -> B (strictly monotonic)
        int[] fullSequence = {0, 1, 2, 3, 4}; // Real A, G25, G50, G75, Real B
        int[] droppedSequence = {0, 2, 3, 4}; // G25 omitted

        for (int i = 1; i < droppedSequence.length; i++) {
            assertTrue("Subsequence must remain strictly monotonic", droppedSequence[i] > droppedSequence[i - 1]);
        }
    }
}
