package io.homo.superresolution.common.upscale.interoplayer;

/** Drain only created resource users; preserve owners if any cleanup fails. */
public final class InteropTeardown {
    private InteropTeardown() {}
    public static void release(boolean resourcesCreated, Runnable drain, Runnable commands,
                               Runnable feature, Runnable resources) {
        if (resourcesCreated) drain.run();
        commands.run();
        feature.run();
        if (resourcesCreated) resources.run();
    }
}
