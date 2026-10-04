package io.homo.superresolution.common.presentation.capture;

/** Immutable receipt published only after the upscaler's successful queue submission.
 * The binary signals belong to that submission, after its last depth/motion read.
 * This is an ordering receipt, not a claim that the GPU has already completed.
 */
public record BorrowedInputReadiness(long captureGeneration, int frame, Submission producer,
                                     Source depth, Source motion) {
    public record Submission(long device, long queue, int family, int index,
                             long command, long generation, long fence) {}
    public record Source(long image, int format, int width, int height, int layout, long ready) {}

    public BorrowedInputReadiness {
        if (captureGeneration <= 0 || producer == null || producer.device == 0 || producer.queue == 0
                || producer.family < 0 || producer.index < 0 || producer.command == 0
                || producer.generation <= 0 || producer.fence == 0) reject("Missing producer submission");
        check(depth); check(motion);
        if (depth.image == motion.image || depth.ready == motion.ready) reject("Aliased input receipt");
    }
    private static void check(Source s) {
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (5), declared by the GL producer signal.
        if (s == null || s.image == 0 || s.ready == 0 || s.width <= 0 || s.height <= 0
                || s.layout != 5) reject("Missing or unsupported borrowed source readiness/layout");
    }
    public void validate(long generation, int logicalFrame, Submission actualProducer,
                         long consumerDevice, long consumerQueue, int consumerFamily, int consumerIndex,
                         Source actualDepth, Source actualMotion, long[] waits) {
        if (generation != captureGeneration || logicalFrame != frame) reject("Stale/wrong-frame readiness");
        if (!producer.equals(actualProducer)) reject("Producer queue/command/fence mismatch");
        if (consumerDevice != producer.device || consumerFamily != producer.family
                || consumerQueue == 0 || consumerIndex < 0
                || (consumerIndex == producer.index) != (consumerQueue == producer.queue))
            reject("Unimplemented queue-family/device ownership transfer or wrong queue identity");
        if (!depth.equals(actualDepth) || !motion.equals(actualMotion)) reject("Source binding/layout changed");
        if (waits == null || !contains(waits, depth.ready) || !contains(waits, motion.ready))
            reject("Producer signals missing from consumer GPU wait");
    }
    private static boolean contains(long[] a, long v) { for (long x : a) if (x == v) return true; return false; }
    public void requireIndependentDestination(long depthImage, long motionImage, int restoredDepthLayout,
                                               int restoredMotionLayout) {
        if (depthImage == 0 || motionImage == 0 || depthImage == motionImage
                || depthImage == depth.image || depthImage == motion.image
                || motionImage == depth.image || motionImage == motion.image
                || restoredDepthLayout != depth.layout || restoredMotionLayout != motion.layout)
            reject("Destination alias or borrowed source layout not restored");
    }
    private static void reject(String why) { throw new IllegalStateException("Borrowed input queue join: " + why); }
}
