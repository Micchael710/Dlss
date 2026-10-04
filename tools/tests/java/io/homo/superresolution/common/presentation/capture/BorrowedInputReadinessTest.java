package io.homo.superresolution.common.presentation.capture;

/** CPU-only contract tests: no GPU completion or capability is simulated. */
public final class BorrowedInputReadinessTest {
    private static int rejected;
    private static void rejects(Runnable action) {
        try { action.run(); } catch (IllegalStateException expected) { rejected++; return; }
        throw new AssertionError("Unsafe contract accepted");
    }
    public static void main(String[] args) {
        var p = new BorrowedInputReadiness.Submission(1, 2, 0, 0, 3, 4, 5);
        var d = new BorrowedInputReadiness.Source(10, 100, 495, 278, 5, 11);
        var m = new BorrowedInputReadiness.Source(20, 83, 495, 278, 5, 21);
        var r = new BorrowedInputReadiness(1, 42, p, d, m);
        long[] waits = {30, 31, 11, 21};
        r.validate(1, 42, p, 1, 6, 0, 1, d, m, waits);
        r.requireIndependentDestination(50, 60, 5, 5);
        rejects(() -> new BorrowedInputReadiness(1, 42, null, d, m));
        rejects(() -> new BorrowedInputReadiness(1, 42, p, null, m));
        rejects(() -> r.validate(1, 43, p, 1, 6, 0, 1, d, m, waits));
        // Reset/capture slot reuse must invalidate an earlier receipt, even for the same frame number.
        rejects(() -> r.validate(2, 42, p, 1, 6, 0, 1, d, m, waits));
        rejects(() -> r.validate(1, 42, new BorrowedInputReadiness.Submission(1, 7, 0, 0, 3, 4, 5), 1, 6, 0, 1, d, m, waits));
        rejects(() -> r.validate(1, 42, new BorrowedInputReadiness.Submission(1, 2, 0, 0, 3, 4, 8), 1, 6, 0, 1, d, m, waits));
        rejects(() -> r.validate(1, 42, p, 1, 6, 1, 1, d, m, waits));
        rejects(() -> r.validate(1, 42, p, 1, 6, 0, 0, d, m, waits));
        rejects(() -> r.validate(1, 42, p, 9, 6, 0, 1, d, m, waits));
        rejects(() -> r.validate(1, 42, p, 1, 6, 0, 1, d, m, new long[]{11}));
        rejects(() -> r.validate(1, 42, p, 1, 6, 0, 1,
                new BorrowedInputReadiness.Source(0, 100, 495, 278, 5, 11), m, waits));
        rejects(() -> r.requireIndependentDestination(10, 60, 5, 5));
        rejects(() -> r.requireIndependentDestination(50, 60, 6, 5));
        if (d.image() != 10 || m.image() != 20 || d.layout() != 5 || m.layout() != 5)
            throw new AssertionError("Source owner was mutated");
        // The tested receipt API uses only comparisons; validation has no waits, copies or resource destruction.
        System.out.println("PASS borrowed GPU handoff CPU contract: accepted exact readiness; " + rejected
                + " unsafe cases rejected; reset freshness, independent destination, source lifetime/layout preserved; no CPU wait API");
    }
}
