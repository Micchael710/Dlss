package io.homo.superresolution.api.registry.framegeneration;

import java.util.Objects;

/** Immutable real-render-frame measurements. Unknown color/luminance values stay unknown. */
public record RealFrameMetadata(long realFrameTimestamp, double realFrameDeltaMs,
                                long monotonicFrameId, long discontinuityEpoch,
                                Size renderSize, Size displaySize, Color color,
                                double viewSpaceToMetersFactor) {
    public RealFrameMetadata {
        Objects.requireNonNull(renderSize);
        Objects.requireNonNull(displaySize);
        Objects.requireNonNull(color);
        if (monotonicFrameId <= 0 || discontinuityEpoch < 0 || realFrameDeltaMs < 0
                || !Double.isFinite(realFrameDeltaMs)) throw new IllegalArgumentException("Invalid frame timing");
    }
    public record Size(int width, int height) {
        public Size { if (width <= 0 || height <= 0) throw new IllegalArgumentException("Invalid size"); }
    }
    public record Color(String format, String transferFunction, Double minLuminance,
                        Double maxLuminance, String source) {
        public Color { Objects.requireNonNull(format); Objects.requireNonNull(transferFunction); Objects.requireNonNull(source); }
        public static Color unknown() { return new Color("UNKNOWN", "UNKNOWN", null, null, "UNKNOWN"); }
    }
    public RealFrameMetadata withColor(Color value) {
        return new RealFrameMetadata(realFrameTimestamp, realFrameDeltaMs, monotonicFrameId,
                discontinuityEpoch, renderSize, displaySize, value, viewSpaceToMetersFactor);
    }
}
