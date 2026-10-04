package io.homo.superresolution.common.upscale.algo.dlss;

/** Startup policy and checks shared by the provider and its CPU tests. */
public final class DlssSrPrerequisites {
    private DlssSrPrerequisites() {}
    public static boolean shouldInitializeVulkan(boolean skip, String selectedAlgorithm) {
        return !skip || "dlss".equals(selectedAlgorithm);
    }
    public static void requireReady(boolean vulkanReady, boolean glInteropReady) {
        if (!vulkanReady) throw new IllegalStateException("NVIDIA DLSS requires an initialized Vulkan device");
        if (!glInteropReady) throw new IllegalStateException("NVIDIA DLSS requires GL/Vulkan memory and semaphore interop");
    }
}
