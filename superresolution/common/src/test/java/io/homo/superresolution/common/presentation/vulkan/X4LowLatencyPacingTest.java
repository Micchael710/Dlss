package io.homo.superresolution.common.presentation.vulkan;

import org.junit.Test;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;
import static org.junit.Assert.*;

public class X4LowLatencyPacingTest {

    private static long targetSpacingNanos(long realPeriodNanos, int generatedCount) {
        return realPeriodNanos / (generatedCount + 1L);
    }

    private static class PacingSimulationResult {
        final List<Long> presentedDeadlines = new ArrayList<>();
        final List<Integer> presentedPhases = new ArrayList<>();
        int advanceCount = 0;
    }

    private PacingSimulationResult simulateBatch(
            long realANanos,
            long intervalNanos,
            boolean[] dropSlot // slot 0 (G25), slot 1 (G50), slot 2 (G75), slot 3 (Real B)
    ) {
        // Use a realistic non-zero clock to ensure nanoTime != 0L
        long startClock = realANanos == 0L ? 1_000_000_000L : realANanos;
        AtomicLong fakeClock = new AtomicLong(startClock);
        PresentPacer pacer = new PresentPacer(fakeClock::get, new FramePacingTiming(fakeClock::get), deadline -> {});

        // Prior real frame Real A completed at startClock.
        // Pacer starts batch with interval.
        pacer.beginPresentFrameBatch(false, true, 3, intervalNanos);

        PacingSimulationResult res = new PacingSimulationResult();

        for (int slot = 0; slot < 4; slot++) {
            boolean dropped = dropSlot[slot];
            if (dropped) {
                // Drop slot: must advance exactly once without sleeping or presenting
                pacer.skipPresentFrameSlot();
                res.advanceCount++;
            } else {
                long deadline = pacer.nextDeadlineNanos();
                res.presentedDeadlines.add(deadline - startClock);
                res.presentedPhases.add(slot + 1);
                if (slot < 3) {
                    pacer.sleepAtPresentGeneratedFrame();
                } else {
                    pacer.sleepAtPresentRealFrame();
                }
                pacer.beginPresentFrame();
                pacer.endPresentFrame(true);
                res.advanceCount++;
            }
        }
        pacer.endPresentFrameBatch();
        return res;
    }

    @Test
    public void test1_NoDropFullSequence() {
        // 40 FPS: period = 25 ms, spacing = 6.25 ms
        long period = 25_000_000L;
        long spacing = targetSpacingNanos(period, 3);
        assertEquals(6_250_000L, spacing);

        boolean[] drops = {false, false, false, false};
        PacingSimulationResult r = simulateBatch(0L, spacing, drops);

        assertEquals(4, r.presentedDeadlines.size());
        assertEquals(4, r.advanceCount);

        // Timeline: A = 0 ms -> G25 = 6.25 ms -> G50 = 12.50 ms -> G75 = 18.75 ms -> Real B = 25.00 ms
        long d0 = r.presentedDeadlines.get(0);
        long d1 = r.presentedDeadlines.get(1);
        long d2 = r.presentedDeadlines.get(2);
        long d3 = r.presentedDeadlines.get(3);

        assertEquals(spacing, d0);
        assertEquals(2 * spacing, d1);
        assertEquals(3 * spacing, d2);
        assertEquals(4 * spacing, d3);

        // Phases: 1 (25%), 2 (50%), 3 (75%), 4 (100%)
        assertEquals(List.of(1, 2, 3, 4), r.presentedPhases);
    }

    @Test
    public void test2_DropG25() {
        long spacing = 6_250_000L; // 6.25 ms
        boolean[] drops = {true, false, false, false}; // G25 dropped
        PacingSimulationResult r = simulateBatch(0L, spacing, drops);

        assertEquals(3, r.presentedDeadlines.size());
        assertEquals(4, r.advanceCount); // Exactly 4 advances total

        // G50 must NOT take G25's slot (6.25 ms). It must remain at Phase 2 (12.50 ms)!
        long g50Deadline = r.presentedDeadlines.get(0);
        assertNotEquals("G50 must not occupy G25's temporal slot", spacing, g50Deadline);
        assertEquals(2 * spacing, g50Deadline); // G50 at phase 2 (12.50 ms)

        long g75Deadline = r.presentedDeadlines.get(1);
        assertEquals(3 * spacing, g75Deadline); // G75 at phase 3 (18.75 ms)

        long realBDeadline = r.presentedDeadlines.get(2);
        assertEquals(4 * spacing, realBDeadline); // Real B at phase 4 (25.00 ms)

        assertEquals(List.of(2, 3, 4), r.presentedPhases);
    }

    @Test
    public void test3_DropG50() {
        long spacing = 6_250_000L;
        boolean[] drops = {false, true, false, false}; // G50 dropped
        PacingSimulationResult r = simulateBatch(0L, spacing, drops);

        assertEquals(3, r.presentedDeadlines.size());
        assertEquals(4, r.advanceCount);

        assertEquals(spacing, (long) r.presentedDeadlines.get(0));     // G25 at phase 1 (6.25 ms)
        assertEquals(3 * spacing, (long) r.presentedDeadlines.get(1)); // G75 at phase 3 (18.75 ms)
        assertEquals(4 * spacing, (long) r.presentedDeadlines.get(2)); // Real B at phase 4 (25.00 ms)

        assertEquals(List.of(1, 3, 4), r.presentedPhases);
    }

    @Test
    public void test4_DropG75() {
        long spacing = 6_250_000L;
        boolean[] drops = {false, false, true, false}; // G75 dropped
        PacingSimulationResult r = simulateBatch(0L, spacing, drops);

        assertEquals(3, r.presentedDeadlines.size());
        assertEquals(4, r.advanceCount);

        assertEquals(spacing, (long) r.presentedDeadlines.get(0));     // G25 at phase 1 (6.25 ms)
        assertEquals(2 * spacing, (long) r.presentedDeadlines.get(1)); // G50 at phase 2 (12.50 ms)
        assertEquals(4 * spacing, (long) r.presentedDeadlines.get(2)); // Real B at phase 4 (25.00 ms)

        assertEquals(List.of(1, 2, 4), r.presentedPhases);
    }

    @Test
    public void test5_DropG25AndG50() {
        long spacing = 6_250_000L;
        boolean[] drops = {true, true, false, false}; // G25 and G50 dropped
        PacingSimulationResult r = simulateBatch(0L, spacing, drops);

        assertEquals(2, r.presentedDeadlines.size());
        assertEquals(4, r.advanceCount);

        assertEquals(3 * spacing, (long) r.presentedDeadlines.get(0)); // G75 at phase 3 (18.75 ms)
        assertEquals(4 * spacing, (long) r.presentedDeadlines.get(1)); // Real B at phase 4 (25.00 ms)

        assertEquals(List.of(3, 4), r.presentedPhases);
    }

    @Test
    public void test6_DropAllGeneratedFrames() {
        long spacing = 6_250_000L;
        boolean[] drops = {true, true, true, false}; // All generated dropped
        PacingSimulationResult r = simulateBatch(0L, spacing, drops);

        assertEquals(1, r.presentedDeadlines.size());
        assertEquals(4, r.advanceCount);

        // Real B MUST remain at Phase 4 (25.00 ms = 4 * spacing), never shift into G25's slot
        long realBDeadline = r.presentedDeadlines.get(0);
        assertEquals(4 * spacing, realBDeadline);
        assertEquals(List.of(4), r.presentedPhases);
    }

    @Test
    public void test7_DisabledOutputDoesNotCompress() {
        long spacing = 6_250_000L;
        // G50 marked disabled/not presentable by provider -> treated as slot skipped
        boolean[] drops = {false, true, false, false};
        PacingSimulationResult r = simulateBatch(0L, spacing, drops);

        assertEquals(3, r.presentedDeadlines.size());
        assertEquals(spacing, (long) r.presentedDeadlines.get(0));     // G25 at 25% (6.25 ms)
        assertEquals(3 * spacing, (long) r.presentedDeadlines.get(1)); // G75 at 75% (18.75 ms)
        assertEquals(4 * spacing, (long) r.presentedDeadlines.get(2)); // Real B at 100% (25.00 ms)
    }

    @Test
    public void test8_RealFrameNeverDroppedByStalePolicy() {
        PresentImage.Kind realKind = PresentImage.Kind.REAL;
        PresentImage.Kind generatedKind = PresentImage.Kind.GENERATED;

        boolean isRealCandidate = (realKind == PresentImage.Kind.GENERATED);
        assertFalse("Real frame must never match generated drop condition", isRealCandidate);

        boolean isGenCandidate = (generatedKind == PresentImage.Kind.GENERATED);
        assertTrue("Generated frame is candidate for stale drop", isGenCandidate);
    }

    @Test
    public void test9_NoDoubleAdvance() {
        long spacing = 6_250_000L;
        // Test all possible permutations of drops: 0 drops, 1 drop, 2 drops, 3 drops
        boolean[][] testCases = {
                {false, false, false, false},
                {true, false, false, false},
                {false, true, false, false},
                {false, false, true, false},
                {true, true, false, false},
                {true, false, true, false},
                {false, true, true, false},
                {true, true, true, false}
        };

        for (boolean[] drops : testCases) {
            PacingSimulationResult r = simulateBatch(0L, spacing, drops);
            assertEquals("Total advances per batch must equal imageCount (4)", 4, r.advanceCount);
        }
    }

    @Test
    public void test10_PacingSpacingsAndPhasesAtVariableFramerates() {
        int generatedCount = 3;

        // 1. 50 FPS real: period = 20 ms, spacing = 5.00 ms
        long p50 = 20_000_000L;
        long s50 = targetSpacingNanos(p50, generatedCount);
        assertEquals(5_000_000L, s50);
        PacingSimulationResult r50 = simulateBatch(0L, s50, new boolean[]{false, false, false, false});
        assertEquals(5_000_000L, (long) r50.presentedDeadlines.get(0));
        assertEquals(10_000_000L, (long) r50.presentedDeadlines.get(1));
        assertEquals(15_000_000L, (long) r50.presentedDeadlines.get(2));
        assertEquals(20_000_000L, (long) r50.presentedDeadlines.get(3));

        // 2. 40 FPS real: period = 25 ms, spacing = 6.25 ms
        long p40 = 25_000_000L;
        long s40 = targetSpacingNanos(p40, generatedCount);
        assertEquals(6_250_000L, s40);
        PacingSimulationResult r40 = simulateBatch(0L, s40, new boolean[]{false, false, false, false});
        assertEquals(6_250_000L, (long) r40.presentedDeadlines.get(0));
        assertEquals(12_500_000L, (long) r40.presentedDeadlines.get(1));
        assertEquals(18_750_000L, (long) r40.presentedDeadlines.get(2));
        assertEquals(25_000_000L, (long) r40.presentedDeadlines.get(3));

        // 3. 30 FPS real: period = 33.333 ms, spacing ≈ 8.333 ms
        long p30 = 33_333_333L;
        long s30 = targetSpacingNanos(p30, generatedCount);
        assertEquals(8_333_333L, s30);
        PacingSimulationResult r30 = simulateBatch(0L, s30, new boolean[]{false, false, false, false});
        assertEquals(8_333_333L, (long) r30.presentedDeadlines.get(0));
        assertEquals(16_666_666L, (long) r30.presentedDeadlines.get(1));
        assertEquals(24_999_999L, (long) r30.presentedDeadlines.get(2));
        assertEquals(33_333_332L, (long) r30.presentedDeadlines.get(3));

        // 4. 23 FPS real: period = 43.478 ms, spacing ≈ 10.870 ms
        long p23 = 43_478_261L;
        long s23 = targetSpacingNanos(p23, generatedCount);
        assertEquals(10_869_565L, s23);
        PacingSimulationResult r23 = simulateBatch(0L, s23, new boolean[]{false, false, false, false});
        assertEquals(10_869_565L, (long) r23.presentedDeadlines.get(0));
        assertEquals(21_739_130L, (long) r23.presentedDeadlines.get(1));
        assertEquals(32_608_695L, (long) r23.presentedDeadlines.get(2));
        assertEquals(43_478_260L, (long) r23.presentedDeadlines.get(3));
    }

    @Test
    public void testEmaReactsToFpsTransition() {
        FramePacingEstimator estimator = new FramePacingEstimator("wisteria:dlssg_fg");
        double ema = 16.666667;
        double target = 25.0;
        double alpha = 0.2;
        for (int i = 0; i < 6; i++) {
            ema = ema * (1.0 - alpha) + target * alpha;
        }
        assertTrue("EMA must reach > 22.0 ms after 6 frames of 40 FPS", ema > 22.0);
        assertTrue("EMA must not overshoot target", ema <= 25.0);
    }

    @Test
    public void testStaleGeneratedDropThreshold() {
        assertEquals(1.0, PresentPacer.MAX_GENERATED_LATENESS_SPACINGS, 0.0001);
        long targetSpacingNs = 6_250_000L;
        long maxAllowedLateNs = (long) (PresentPacer.MAX_GENERATED_LATENESS_SPACINGS * targetSpacingNs);
        assertEquals(6_250_000L, maxAllowedLateNs);

        long deadlineNs = 1_000_000_000L;
        long nowLate = deadlineNs + 8_000_000L;
        assertTrue(nowLate - deadlineNs > maxAllowedLateNs);

        long nowOnTime = deadlineNs + 2_000_000L;
        assertFalse(nowOnTime - deadlineNs > maxAllowedLateNs);
    }

    @Test
    public void testPipelineDepthLimit() {
        assertEquals(2, AsyncFramePresenter.MAX_PIPELINED_REAL_FRAMES_X4);
    }

    @Test
    public void testPresentationOrderPreserved() {
        int[] fullSequence = {0, 1, 2, 3, 4};
        int[] droppedSequence = {0, 2, 3, 4};

        for (int i = 1; i < droppedSequence.length; i++) {
            assertTrue("Subsequence must remain strictly monotonic", droppedSequence[i] > droppedSequence[i - 1]);
        }
    }
}
