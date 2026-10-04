package io.homo.superresolution.api.registry.framegeneration;

/** Optional cross-API input submission; the presenter still owns all presentation. */
public interface FrameGenerationSubmissionPlan {
    /** Consumes capture-ready semaphores once and returns a GPU timeline wait for output work. */
    TimelineWait submitInputs();

    /** True once input work may have reached the GPU; failure then requires terminal drain. */
    boolean inputsSubmitted();

    record TimelineWait(long semaphore, long value) {
        public TimelineWait {
            if (semaphore == 0 || value <= 0) throw new IllegalArgumentException("Invalid timeline wait");
        }
    }
}
