package org.ireallywanttosleep.wisteria.fsr;

import io.homo.superresolution.api.registry.framegeneration.*;
import io.homo.superresolution.common.framegeneration.FrameGenerationMode;
import io.homo.superresolution.common.framegeneration.constants.FrameGenerationConstants;

public record FsrInputSnapshot(String providerId, int logicalFrameIndex, FrameGenerationMode mode,
                               FrameGenerationConstants constants, boolean historyResetRequested,
                               RealFrameMetadata metadata) implements ProviderInputSnapshot {}
