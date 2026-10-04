/*
 * Super Resolution
 * Copyright (c) 2026. 187J3X1-114514
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

package io.homo.superresolution.common.framegeneration.constants;

import io.homo.superresolution.api.SuperResolutionAPI;
import io.homo.superresolution.api.event.AlgorithmDispatchEvent;
import io.homo.superresolution.common.SuperResolution;
import io.homo.superresolution.common.minecraft.MinecraftUtils;
import io.homo.superresolution.common.upscale.DispatchResource;
import io.homo.superresolution.common.workmode.SRWorkModeManager;
import io.homo.superresolution.common.workmode.SRWorkModeProvider;
import io.homo.superresolution.common.workmode.SRWorkModeState;
import net.minecraft.client.Minecraft;

import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;

public final class FGConstantsFeature {
    private static final int CONSTANTS_CAPACITY = 8;
    private static final Map<Integer, FrameGenerationConstants> CONSTANTS = new LinkedHashMap<>();
    private static final Map<Integer, io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata> METADATA = new LinkedHashMap<>();
    private static final RealFrameClock REAL_FRAME_CLOCK = new RealFrameClock(System::nanoTime);

    private static boolean registered;
    private static boolean initialized;
    private static boolean renderFrameOpen;
    private static boolean dispatchCapturedThisRender;
    private static boolean captureHistoryInvalid = true;
    private static boolean captureFailureReported;
    private static CapturedCameraFrame previousCameraFrame;

    private FGConstantsFeature() {
    }

    public static synchronized void register() {
        if (registered) {
            return;
        }
        SuperResolutionAPI.EVENT_BUS.addListener(FGConstantsFeature::onAlgorithmDispatch);
        registered = true;
    }

    public static synchronized void initialize() {
        clearState();
        initialized = true;
    }

    public static synchronized void beginRenderFrame() {
        if (!initialized) {
            return;
        }
        renderFrameOpen = true;
        dispatchCapturedThisRender = false;
    }

    public static synchronized void endRenderFrame() {
        if (!initialized || !renderFrameOpen) {
            return;
        }
        renderFrameOpen = false;
        if (!dispatchCapturedThisRender) {
            invalidateHistoryInternal();
        }
    }

    public static synchronized FrameGenerationConstants getConstants(int frameIndex) {
        return CONSTANTS.get(frameIndex);
    }

    public static synchronized io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata getMetadata(int frameIndex) {
        return METADATA.get(frameIndex);
    }

    public static synchronized void invalidateHistory() {
        if (initialized) {
            invalidateHistoryInternal();
            captureFailureReported = false;
        }
    }

    public static synchronized void shutdown() {
        clearState();
        initialized = false;
    }

    private static synchronized void onAlgorithmDispatch(AlgorithmDispatchEvent event) {
        if (!initialized) {
            return;
        }
        dispatchCapturedThisRender = true;
        if (event == null || event.getDispatchResource() == null) {
            invalidateHistoryInternal();
            return;
        }

        DispatchResource dispatch = event.getDispatchResource();
        Minecraft minecraft = Minecraft.getInstance();
        if (minecraft.level == null) {
            invalidateHistoryInternal();
            return;
        }

        SRWorkModeProvider workModeProvider = SRWorkModeManager.getCurrentProvider();
        SRWorkModeState workModeState = SRWorkModeManager.getCurrentState();
        if (workModeProvider == null) {
            return;
        }
        String workModeId = workModeProvider.id();
        boolean motionVectorsJittered = workModeState.initializationDescription().isMotionJittered();
        boolean cameraMotionIncluded =
                SRWorkModeManager.SHADER_COMPAT.equals(workModeId)
                        && workModeState.shaderPackInUse()
                        && !workModeState.shaderPackLoading();
        try {
            float aspectRatio = FGConstantsBuilder.resolveAspectRatio(
                    dispatch.projectionMatrix(),
                    dispatch.renderWidth(),
                    dispatch.renderHeight()
            );
            FGConstantsBuilder.CameraFrame cameraFrame = MinecraftCameraState.capture(
                    dispatch.frameCount(),
                    dispatch.projectionMatrix(),
                    MinecraftUtils.getCameraNear(),
                    MinecraftUtils.getCameraFar(),
                    aspectRatio
            );
            ContinuityKey continuityKey = new ContinuityKey(
                    dispatch.renderWidth(),
                    dispatch.renderHeight(),
                    dispatch.screenWidth(),
                    dispatch.screenHeight(),
                    workModeId,
                    workModeState.internalTextureFormat(),
                    workModeState.motionVectorPreprocessingFunction(),
                    cameraMotionIncluded,
                    motionVectorsJittered
            );
            boolean reset = captureHistoryInvalid
                    || previousCameraFrame == null
                    || dispatch.frameCount() != previousCameraFrame.frame.frameIndex() + 1
                    || !continuityKey.equals(previousCameraFrame.continuityKey);

            FrameGenerationConstants constants = FGConstantsBuilder.build(
                    cameraFrame,
                    reset ? null : previousCameraFrame.frame,
                    reset,
                    cameraMotionIncluded,
                    motionVectorsJittered,
                    dispatch.jitterOffset().x,
                    dispatch.jitterOffset().y,
                    1.0F,
                    1.0F
            );
            CONSTANTS.put(dispatch.frameCount(), constants);
            var metadata = REAL_FRAME_CLOCK.capture(dispatch.frameCount(),
                    new io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata.Size(dispatch.renderWidth(), dispatch.renderHeight()),
                    new io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata.Size(dispatch.screenWidth(), dispatch.screenHeight()), reset);
            // MinecraftCameraState uses vanilla rotation plus world positions in blocks,
            // without view-space scaling. Minecraft's documented block convention is 1m.
            // This derivation does not apply to arbitrary external/custom camera sources.
            metadata = new io.homo.superresolution.api.registry.framegeneration.RealFrameMetadata(
                    metadata.realFrameTimestamp(), metadata.realFrameDeltaMs(), metadata.monotonicFrameId(),
                    metadata.discontinuityEpoch(), metadata.renderSize(), metadata.displaySize(), metadata.color(), 1.0);
            METADATA.put(dispatch.frameCount(), metadata);
            if (Boolean.getBoolean("sr.baselineDiagnostics") && metadata.monotonicFrameId() <= 5) {
                SuperResolution.LOGGER.info("SR_FG_INPUT frame={} timestampNs={} deltaMs={} epoch={} render={} display={} jitter={},{} cameraNear={} cameraFar={} fovRadians={} depthInverted={} MV=previousMinusCurrentUV_Yflipped meters=1_MinecraftBlockConvention",
                        metadata.monotonicFrameId(), metadata.realFrameTimestamp(), metadata.realFrameDeltaMs(),
                        metadata.discontinuityEpoch(), metadata.renderSize(), metadata.displaySize(),
                        constants.jitterOffsetX(), constants.jitterOffsetY(), constants.cameraNear(), constants.cameraFar(),
                        constants.cameraFov(), constants.depthInverted());
            }
            trimConstants();
            previousCameraFrame = new CapturedCameraFrame(cameraFrame, continuityKey);
            captureHistoryInvalid = false;
            captureFailureReported = false;
        } catch (RuntimeException exception) {
            invalidateHistoryInternal();
            if (!captureFailureReported) {
                SuperResolution.LOGGER.warn("Failed to build DLSS-G constants", exception);
                if (Boolean.getBoolean("wisteria.dlssg.enabled")) {
                    SuperResolution.LOGGER.warn("DLSSG_MATRIX_EVIDENCE logicalFrame={} render={}x{} display={}x{} projection={} previousProjection={} modelView={} jitter={} resetHistory={}",
                            dispatch.frameCount(), dispatch.renderWidth(), dispatch.renderHeight(), dispatch.screenWidth(), dispatch.screenHeight(),
                            java.util.Arrays.toString(dispatch.projectionMatrix().get(new float[16])),
                            java.util.Arrays.toString(dispatch.lastProjectionMatrix().get(new float[16])),
                            java.util.Arrays.toString(dispatch.modelViewMatrix().get(new float[16])), dispatch.jitterOffset(), captureHistoryInvalid);
                }
                captureFailureReported = true;
            }
        }
    }

    private static void invalidateHistoryInternal() {
        captureHistoryInvalid = true;
        REAL_FRAME_CLOCK.invalidateHistory();
        previousCameraFrame = null;
    }

    private static void trimConstants() {
        while (CONSTANTS.size() > CONSTANTS_CAPACITY) {
            Iterator<Integer> iterator = CONSTANTS.keySet().iterator();
            if (!iterator.hasNext()) {
                return;
            }
            iterator.next();
            iterator.remove();
        }
        while (METADATA.size() > CONSTANTS_CAPACITY) {
            METADATA.remove(METADATA.keySet().iterator().next());
        }
    }

    private static void clearState() {
        CONSTANTS.clear();
        METADATA.clear();
        renderFrameOpen = false;
        dispatchCapturedThisRender = false;
        captureHistoryInvalid = true;
        captureFailureReported = false;
        previousCameraFrame = null;
    }

    private record CapturedCameraFrame(
            FGConstantsBuilder.CameraFrame frame,

            ContinuityKey continuityKey
    ) {
    }

    private record ContinuityKey(
            int renderWidth,

            int renderHeight,

            int screenWidth,

            int screenHeight,

            String workModeId,

            Object internalTextureFormat,

            String motionVectorPreprocessingFunction,

            boolean cameraMotionIncluded,

            boolean motionVectorsJittered
    ) {
    }
}
